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
		std::unique_ptr<vk::CopyDescriptorSet[]> descriptorCopies;

		usize descriptorWriteEnd = 0;
		usize descriptorCopyEnd = 0;

		DescriptorUpdateBatch(vk::Device device, usize descriptorWriteMax, usize descriptorCopyMax)
			: device(device), descriptorWriteMax(descriptorWriteMax), descriptorCopyMax(descriptorCopyMax) {}

	  public:
		~DescriptorUpdateBatch() = default;

		DescriptorUpdateBatch(DescriptorUpdateBatch&&) = default;

		void flush();

		void addImage(
			vk::DescriptorSet targetDescriptor, u8 targetBinding, vk::ImageView imageView, vk::ImageLayout imageLayout = vk::ImageLayout::eGeneral
		);
		void addSampler(vk::DescriptorSet targetDescriptor, u8 targetBinding, vk::Sampler sampler);

		void addImageSampler(
			vk::DescriptorSet targetDescriptor, u8 targetBinding, vk::ImageView imageView, vk::Sampler sampler,
			vk::ImageLayout imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
		);
		void addBuffer(
			vk::DescriptorSet targetDescriptor, u8 targetBinding, vk::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize size = VK_WHOLE_SIZE
		);

		void copyBinding(
			vk::DescriptorSet sourceDescriptor, vk::DescriptorSet targetDescriptor, u8 sourceBinding, u8 targetBinding, u8 sourceArrayElement = 0,
			u8 targetArrayElement = 0, u8 descriptorCount = 1
		);

		static std::optional<DescriptorUpdateBatch> create(vk::Device device, usize descriptorWriteMax = 256, usize descriptorCopyMax = 256);
	};
}  // namespace Vulkan