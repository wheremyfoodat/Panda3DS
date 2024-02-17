#pragma once

#include <memory>
#include <optional>
#include <variant>

#include "helpers.hpp"
#include "vk_api.hpp"

namespace Vulkan {
	// Implements a re-usable structure for batching up descriptor writes with a
	// finite amount of space for both convenience and to reduce the overall amount
	// of API calls to `vkUpdateDescriptorSets`
	class DescriptorUpdateBatch {
	  private:
		const vk::Device device;

		const usize descriptorWriteMax;
		const usize descriptorCopyMax;

		using DescriptorInfoUnion = std::variant<vk::DescriptorImageInfo, vk::DescriptorBufferInfo, vk::BufferView>;

		// Todo: Maybe some kind of hash so that these structures can be re-used
		// among descriptor writes.
		std::unique_ptr<DescriptorInfoUnion[]> descriptorInfos;
		std::unique_ptr<vk::WriteDescriptorSet[]> descriptorWrites;
		usize descriptorWriteEnd = 0;

	  public:
        DescriptorUpdateBatch(vk::Device device, usize descriptorWriteMax = 256, usize descriptorCopyMax = 256);
		~DescriptorUpdateBatch() = default;

		DescriptorUpdateBatch(DescriptorUpdateBatch&&) = default;

		void flush();

		void addImage(
			vk::DescriptorSet targetDescriptor, u8 targetBinding, vk::ImageView imageView, vk::ImageLayout imageLayout = vk::ImageLayout::eGeneral
		);
		void addSampler(vk::DescriptorSet targetDescriptor, u8 targetBinding, vk::Sampler sampler);

		void addImageSampler(
            vk::DescriptorSet targetDescriptor, u8 targetBinding, u8 targetArrayIndex, vk::ImageView imageView, vk::Sampler sampler,
            vk::ImageLayout imageLayout = vk::ImageLayout::eGeneral
		);
		void addBuffer(
			vk::DescriptorSet targetDescriptor, u8 targetBinding, vk::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize size = VK_WHOLE_SIZE
		);
	};
}  // namespace Vulkan
