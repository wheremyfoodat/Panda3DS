#pragma once

#include <span>
#include <type_traits>
#include <utility>

#include "helpers.hpp"
#include "vk_api.hpp"

namespace Vulkan {

	// Will try to find a memory type that is suitable for the given requirements.
	// Returns -1 if no suitable memory type was found.
	s32 findMemoryTypeIndex(
		vk::PhysicalDevice physicalDevice, u32 memoryTypeMask, vk::MemoryPropertyFlags memoryProperties,
		vk::MemoryPropertyFlags memoryExcludeProperties = vk::MemoryPropertyFlagBits::eProtected
	);

	// Given an array of valid Vulkan image-handles or buffer-handles, these
	// functions will allocate a single block of device-memory for all of them
	// and bind them consecutively.
	// There may be a case that all the buffers or images cannot be allocated
	// to the same device memory due to their required memory-type.
	std::tuple<vk::Result, vk::UniqueDeviceMemory> commitImageHeap(
		vk::Device device, vk::PhysicalDevice physicalDevice, const std::span<const vk::Image> images,
		vk::MemoryPropertyFlags memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal,
		vk::MemoryPropertyFlags memoryExcludeProperties = vk::MemoryPropertyFlagBits::eProtected
	);

	std::tuple<vk::Result, vk::UniqueDeviceMemory> commitBufferHeap(
		vk::Device device, vk::PhysicalDevice physicalDevice, const std::span<const vk::Buffer> buffers,
		vk::MemoryPropertyFlags memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal,
		vk::MemoryPropertyFlags memoryExcludeProperties = vk::MemoryPropertyFlagBits::eProtected
	);

}  // namespace Vulkan