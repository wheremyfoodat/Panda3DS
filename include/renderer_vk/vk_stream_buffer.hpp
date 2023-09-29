#pragma once
#include "vk_instance.hpp"
#include "vk_scheduler.hpp"
#include <vk_mem_alloc.h>

namespace Vulkan {

template <std::size_t MapGranularity>
class StreamBuffer {
  public:
    explicit StreamBuffer(const Instance& instance, Scheduler& scheduler, vk::BufferUsageFlags usage, u32 size)
        : instance(instance), scheduler(scheduler), stream_buffer_size(size) {
        assert(size >= MapGranularity);
        assert(size % MapGranularity == 0);
        timestamps.resize(size / MapGranularity);

        const vk::BufferCreateInfo ci = {
            .size = size,
            .usage = usage,
        };

        const VmaAllocationCreateInfo alloc_ci = {
            .flags = VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT |
                     VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
            .requiredFlags = 0,
            .preferredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            .memoryTypeBits = 0,
            .pool = VK_NULL_HANDLE,
            .pUserData = nullptr,
            .priority = 0.f,
        };

		VkBuffer handle{};
		VmaAllocationInfo alloc_info{};
		VkMemoryPropertyFlags property_flags{};
		VkBufferCreateInfo vma_ci = static_cast<VkBufferCreateInfo>(ci);

		if (vmaCreateBuffer(instance.getAllocator(), &vma_ci, &alloc_ci, &handle, &allocation, &alloc_info) != VK_SUCCESS) {
			Helpers::panic("[vulkan] Unable to create stream buffer!\n");
			return;
		}

		buffer = vk::Buffer{handle};
		vmaGetAllocationMemoryProperties(instance.getAllocator(), allocation, &property_flags);
		u8* data = reinterpret_cast<u8*>(alloc_info.pMappedData);
		mapped = std::span<u8>{data, ci.size};

		// TODO: Handle non coherent types with vkFlushMappedMemoryRanges
		assert(property_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		assert(data);
	}

	~StreamBuffer() {
		vmaDestroyBuffer(instance.getAllocator(), buffer, allocation);
	}

	vk::Buffer getHandle() const {
		return buffer;
	}

	std::pair<std::span<u8>, u32> Map(u32 size) {
		assert(size % MapGranularity == 0);
		assert(size <= stream_buffer_size);

		const u32 offset = currentRegion * MapGranularity;
		const u32 numRegions = size / MapGranularity;
		const u32 endRegion = currentRegion + numRegions;

		// If we run out of space to map, start over from the beginning.
		if (endRegion >= timestamps.size()) {
			currentRegion = 0;
		}

		// Ensure regions we will map are not used by the GPU.
		for (u32 region = currentRegion; region < currentRegion + numRegions; region++) {
			scheduler.waitFor(timestamps[region]);
			timestamps[region] = scheduler.getCpuCounter();
		}

		// Map...
		const u32 newOffset = MapGranularity * currentRegion;
		currentRegion += numRegions;
		return std::make_pair(mapped.subspan(newOffset, size), newOffset);
	}

  private:
    const Instance& instance;
    Scheduler& scheduler;
    vk::Buffer buffer;
    VmaAllocation allocation;
    std::span<u8> mapped{};
    u32 stream_buffer_size{};
    std::vector<u64> timestamps;
    u32 currentRegion{};
};

}
