#include "renderer_vk/vk_descriptor_heap.hpp"
#include "renderer_vk/vk_scheduler.hpp"

#include <algorithm>
#include <optional>
#include <unordered_map>

namespace Vulkan {

	DescriptorHeap::DescriptorHeap(vk::Device device, Scheduler& scheduler) : device(device), scheduler(scheduler) {}

	std::optional<vk::DescriptorSet> DescriptorHeap::allocateDescriptorSet() {
		// Find a free slot
		scheduler.refresh();
		const auto freeSlot = std::find_if(timestamps.begin(), timestamps.end(), [this](u64 tick) {
			return scheduler.isFree(tick);
		});

		// If there is no free slot, return
		if (freeSlot == timestamps.end()) {
			return std::nullopt;
		}

		// Mark the slot as allocated
		*freeSlot = scheduler.getCpuCounter();

		const u16 index = static_cast<u16>(std::distance(timestamps.begin(), freeSlot));
		vk::UniqueDescriptorSet& newDescriptorSet = descriptorSets[index];

		if (!newDescriptorSet) {
			// Descriptor set doesn't exist yet. Allocate a new one
			vk::DescriptorSetAllocateInfo allocateInfo = {};

			allocateInfo.descriptorPool = descriptorPool.get();
			allocateInfo.pSetLayouts = &descriptorSetLayout.get();
			allocateInfo.descriptorSetCount = 1;

			if (auto AllocateResult = device.allocateDescriptorSetsUnique(allocateInfo); AllocateResult.result == vk::Result::eSuccess) {
				newDescriptorSet = std::move(AllocateResult.value[0]);
			} else {
				// Error allocating descriptor set
				return std::nullopt;
			}
		}

		return newDescriptorSet.get();
	}

	std::optional<DescriptorHeap> DescriptorHeap::create(
		vk::Device device, Scheduler& scheduler, std::span<const vk::DescriptorSetLayoutBinding> bindings, u16 descriptorHeapCount
	) {
		DescriptorHeap newDescriptorHeap(device, scheduler);

		// Create a histogram of each of the descriptor types and how many of each
		// the pool should have
		// Todo: maybe keep this around as a hash table to do more dynamic
		// allocations of descriptor sets rather than allocating them all up-front
		std::vector<vk::DescriptorPoolSize> poolSizes;
		{
			std::unordered_map<vk::DescriptorType, u16> descriptorTypeCounts;

			for (const auto& binding : bindings) {
				descriptorTypeCounts[binding.descriptorType] += binding.descriptorCount;
			}
			for (const auto& descriptorTypeCount : descriptorTypeCounts) {
				poolSizes.push_back(vk::DescriptorPoolSize(descriptorTypeCount.first, descriptorTypeCount.second * descriptorHeapCount));
			}
		}

		// Create descriptor pool
		{
			vk::DescriptorPoolCreateInfo poolInfo;
			poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
			poolInfo.maxSets = descriptorHeapCount;
			poolInfo.pPoolSizes = poolSizes.data();
			poolInfo.poolSizeCount = poolSizes.size();
			if (auto createResult = device.createDescriptorPoolUnique(poolInfo); createResult.result == vk::Result::eSuccess) {
				newDescriptorHeap.descriptorPool = std::move(createResult.value);
			} else {
				return std::nullopt;
			}
		}

		// Create descriptor set layout
		{
			vk::DescriptorSetLayoutCreateInfo layoutInfo;
			layoutInfo.pBindings = bindings.data();
			layoutInfo.bindingCount = bindings.size();

			if (auto createResult = device.createDescriptorSetLayoutUnique(layoutInfo); createResult.result == vk::Result::eSuccess) {
				newDescriptorHeap.descriptorSetLayout = std::move(createResult.value);
			} else {
				return std::nullopt;
			}
		}

		newDescriptorHeap.descriptorSets.resize(descriptorHeapCount);
		newDescriptorHeap.timestamps.resize(descriptorHeapCount);
		newDescriptorHeap.bindings.assign(bindings.begin(), bindings.end());

		return {std::move(newDescriptorHeap)};
	}
}  // namespace Vulkan
