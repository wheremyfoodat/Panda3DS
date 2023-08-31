#include "renderer_vk/vk_descriptor_heap.hpp"

#include <algorithm>
#include <optional>
#include <unordered_map>

namespace Vulkan {

	DescriptorHeap::DescriptorHeap(vk::Device device) : device(device) {}

	std::optional<vk::DescriptorSet> DescriptorHeap::allocateDescriptorSet() {
		// Find a free slot
		const auto freeSlot = std::find(allocationMap.begin(), allocationMap.end(), false);

		// If there is no free slot, return
		if (freeSlot == allocationMap.end()) {
			return std::nullopt;
		}

		// Mark the slot as allocated
		*freeSlot = true;

		const u16 index = static_cast<u16>(std::distance(allocationMap.begin(), freeSlot));

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

	bool DescriptorHeap::freeDescriptorSet(vk::DescriptorSet Set) {
		// Find the descriptor set
		const auto found =
			std::find_if(descriptorSets.begin(), descriptorSets.end(), [&Set](const auto& CurSet) -> bool { return CurSet.get() == Set; });

		// If the descriptor set is not found, return
		if (found == descriptorSets.end()) {
			return false;
		}

		// Mark the slot as free
		const u16 index = static_cast<u16>(std::distance(descriptorSets.begin(), found));

		allocationMap[index] = false;

		return true;
	}

	std::optional<DescriptorHeap> DescriptorHeap::create(
		vk::Device device, std::span<const vk::DescriptorSetLayoutBinding> bindings, u16 descriptorHeapCount
	) {
		DescriptorHeap newDescriptorHeap(device);

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
		newDescriptorHeap.allocationMap.resize(descriptorHeapCount);

		newDescriptorHeap.bindings.assign(bindings.begin(), bindings.end());

		return {std::move(newDescriptorHeap)};
	}
}  // namespace Vulkan