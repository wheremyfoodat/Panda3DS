#pragma once

#include <optional>
#include <span>

#include "helpers.hpp"
#include "vk_api.hpp"

namespace Vulkan {
	// Implements a basic heap of descriptor sets given a layout of particular
	// bindings. Create a descriptor set by providing a list of bindings and it will
	// automatically create both the pool, layout, and maintail a heap of descriptor
	// sets. Descriptor sets will be reused and recycled. Assume that newly
	// allocated descriptor sets are in an undefined state.
	class DescriptorHeap {
	  private:
		const vk::Device device;

		vk::UniqueDescriptorPool descriptorPool;
		vk::UniqueDescriptorSetLayout descriptorSetLayout;
		std::vector<vk::UniqueDescriptorSet> descriptorSets;

		std::vector<vk::DescriptorSetLayoutBinding> bindings;

		std::vector<bool> allocationMap;

		explicit DescriptorHeap(vk::Device device);

	  public:
		~DescriptorHeap() = default;

		DescriptorHeap(DescriptorHeap&&) = default;

		const vk::DescriptorPool& getDescriptorPool() const { return descriptorPool.get(); };

		const vk::DescriptorSetLayout& getDescriptorSetLayout() const { return descriptorSetLayout.get(); };

		const std::span<const vk::UniqueDescriptorSet> getDescriptorSets() const { return descriptorSets; };

		std::span<const vk::DescriptorSetLayoutBinding> getBindings() const { return bindings; };

		std::optional<vk::DescriptorSet> allocateDescriptorSet();
		bool freeDescriptorSet(vk::DescriptorSet set);

		static std::optional<DescriptorHeap> create(
			vk::Device device, std::span<const vk::DescriptorSetLayoutBinding> bindings, u16 descriptorHeapCount = 1024
		);
	};
}  // namespace Vulkan