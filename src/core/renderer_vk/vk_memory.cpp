#include "renderer_vk/vk_memory.hpp"

namespace Vulkan {

	static constexpr vk::DeviceSize alignUp(vk::DeviceSize value, std::size_t size) {
		const vk::DeviceSize mod = static_cast<vk::DeviceSize>(value % size);
		value -= mod;
		return static_cast<vk::DeviceSize>(mod == vk::DeviceSize{0} ? value : value + size);
	}

	// Given a speculative heap-allocation, defined by its current size and
	// memory-type bits, appends a memory requirements structure to it, updating
	// both the size and the required memory-type-bits. Returns the offset within
	// the heap for the current MemoryRequirements Todo: Sun Apr 23 13:28:25 PDT
	// 2023 Rather than using a running-size of the heap, look at all of the memory
	// requests and optimally create a packing for all of the offset and alignment
	// requirements. Such as by satisfying all of the largest alignments first, and
	// then the smallest, to reduce padding
	static vk::DeviceSize commitMemoryRequestToHeap(
		const vk::MemoryRequirements& curMemoryRequirements, vk::DeviceSize& curHeapEnd, u32& curMemoryTypeBits, vk::DeviceSize sizeAlignment
	) {
		// Accumulate a mask of all the memory types that satisfies each of the
		// handles
		curMemoryTypeBits &= curMemoryRequirements.memoryTypeBits;

		// Pad up the memory sizes so they are not considered aliasing
		const vk::DeviceSize curMemoryOffset = alignUp(curHeapEnd, curMemoryRequirements.alignment);
		// Pad the size by the required size-alignment.
		// Intended for BufferImageGranularity
		const vk::DeviceSize curMemorySize = alignUp(curMemoryRequirements.size, sizeAlignment);

		curHeapEnd = (curMemoryOffset + curMemorySize);
		return curMemoryOffset;
	}

	s32 findMemoryTypeIndex(
		vk::PhysicalDevice physicalDevice, u32 memoryTypeMask, vk::MemoryPropertyFlags memoryProperties,
		vk::MemoryPropertyFlags memoryExcludeProperties
	) {
		const vk::PhysicalDeviceMemoryProperties deviceMemoryProperties = physicalDevice.getMemoryProperties();
		// Iterate the physical device's memory types until we find a match
		for (std::size_t i = 0; i < deviceMemoryProperties.memoryTypeCount; i++) {
			if(
			// Is within memory type mask
			(((memoryTypeMask >> i) & 0b1) == 0b1) &&
			// Has property flags
			(deviceMemoryProperties.memoryTypes[i].propertyFlags
			 & memoryProperties)
				== memoryProperties
			&&
			// None of the excluded properties are enabled
			!(deviceMemoryProperties.memoryTypes[i].propertyFlags
			  & memoryExcludeProperties) )
		{
				return static_cast<u32>(i);
			}
		}

		return -1;
	}

	std::tuple<vk::Result, vk::UniqueDeviceMemory> commitImageHeap(
		vk::Device device, vk::PhysicalDevice physicalDevice, const std::span<const vk::Image> images, vk::MemoryPropertyFlags memoryProperties,
		vk::MemoryPropertyFlags memoryExcludeProperties
	) {
		vk::MemoryAllocateInfo imageHeapAllocInfo = {};
		u32 imageHeapMemoryTypeBits = 0xFFFFFFFF;
		std::vector<vk::BindImageMemoryInfo> imageHeapBinds;

		const vk::DeviceSize bufferImageGranularity = physicalDevice.getProperties().limits.bufferImageGranularity;

		for (const vk::Image& curImage : images) {
			const vk::DeviceSize curBindOffset = commitMemoryRequestToHeap(
				device.getImageMemoryRequirements(curImage), imageHeapAllocInfo.allocationSize, imageHeapMemoryTypeBits, bufferImageGranularity
			);

			if (imageHeapMemoryTypeBits == 0) {
				// No possible memory heap for all of the images to share
				return std::make_tuple(vk::Result::eErrorOutOfDeviceMemory, vk::UniqueDeviceMemory());
			}

			// Put nullptr for the device memory for now
			imageHeapBinds.emplace_back(vk::BindImageMemoryInfo{curImage, nullptr, curBindOffset});
		}

		const s32 memoryTypeIndex = findMemoryTypeIndex(physicalDevice, imageHeapMemoryTypeBits, memoryProperties, memoryExcludeProperties);

		if (memoryTypeIndex < 0) {
			// Unable to find a memory heap that satisfies all the images
			return std::make_tuple(vk::Result::eErrorOutOfDeviceMemory, vk::UniqueDeviceMemory());
		}

		imageHeapAllocInfo.memoryTypeIndex = memoryTypeIndex;

		vk::UniqueDeviceMemory imageHeapMemory = {};

		if (auto allocResult = device.allocateMemoryUnique(imageHeapAllocInfo); allocResult.result == vk::Result::eSuccess) {
			imageHeapMemory = std::move(allocResult.value);
		} else {
			return std::make_tuple(allocResult.result, vk::UniqueDeviceMemory());
		}

		// Assign the device memory to the bindings
		for (vk::BindImageMemoryInfo& curBind : imageHeapBinds) {
			curBind.memory = imageHeapMemory.get();
		}

		// Now bind them all in one call
		if (const vk::Result bindResult = device.bindImageMemory2(imageHeapBinds); bindResult == vk::Result::eSuccess) {
			// Binding memory succeeded
		} else {
			return std::make_tuple(bindResult, vk::UniqueDeviceMemory());
		}

		return std::make_tuple(vk::Result::eSuccess, std::move(imageHeapMemory));
	}

	std::tuple<vk::Result, vk::UniqueDeviceMemory> commitBufferHeap(
		vk::Device device, vk::PhysicalDevice physicalDevice, const std::span<const vk::Buffer> buffers, vk::MemoryPropertyFlags memoryProperties,
		vk::MemoryPropertyFlags memoryExcludeProperties
	) {
		vk::MemoryAllocateInfo bufferHeapAllocInfo = {};
		u32 bufferHeapMemoryTypeBits = 0xFFFFFFFF;
		std::vector<vk::BindBufferMemoryInfo> bufferHeapBinds;

		const vk::DeviceSize bufferImageGranularity = physicalDevice.getProperties().limits.bufferImageGranularity;

		for (const vk::Buffer& curBuffer : buffers) {
			const vk::DeviceSize curBindOffset = commitMemoryRequestToHeap(
				device.getBufferMemoryRequirements(curBuffer), bufferHeapAllocInfo.allocationSize, bufferHeapMemoryTypeBits, bufferImageGranularity
			);

			if (bufferHeapMemoryTypeBits == 0) {
				// No possible memory heap for all of the buffers to share
				return std::make_tuple(vk::Result::eErrorOutOfDeviceMemory, vk::UniqueDeviceMemory());
			}

			// Put nullptr for the device memory for now
			bufferHeapBinds.emplace_back(vk::BindBufferMemoryInfo{curBuffer, nullptr, curBindOffset});
		}

		const s32 memoryTypeIndex = findMemoryTypeIndex(physicalDevice, bufferHeapMemoryTypeBits, memoryProperties, memoryExcludeProperties);

		if (memoryTypeIndex < 0) {
			// Unable to find a memory heap that satisfies all the buffers
			return std::make_tuple(vk::Result::eErrorOutOfDeviceMemory, vk::UniqueDeviceMemory());
		}

		bufferHeapAllocInfo.memoryTypeIndex = memoryTypeIndex;

		vk::UniqueDeviceMemory bufferHeapMemory = {};

		if (auto allocResult = device.allocateMemoryUnique(bufferHeapAllocInfo); allocResult.result == vk::Result::eSuccess) {
			bufferHeapMemory = std::move(allocResult.value);
		} else {
			return std::make_tuple(allocResult.result, vk::UniqueDeviceMemory());
		}

		// Assign the device memory to the bindings
		for (vk::BindBufferMemoryInfo& curBind : bufferHeapBinds) {
			curBind.memory = bufferHeapMemory.get();
		}

		// Now bind them all in one call
		if (const vk::Result bindResult = device.bindBufferMemory2(bufferHeapBinds); bindResult == vk::Result::eSuccess) {
			// Binding memory succeeded
		} else {
			return std::make_tuple(bindResult, vk::UniqueDeviceMemory());
		}

		return std::make_tuple(vk::Result::eSuccess, std::move(bufferHeapMemory));
	}

}  // namespace Vulkan