#include "renderer_vk/vk_descriptor_update_batch.hpp"

#include <memory>
#include <span>

namespace Vulkan {

	void DescriptorUpdateBatch::flush() {
		device.updateDescriptorSets({std::span(descriptorWrites.get(), descriptorWriteEnd)}, {std::span(descriptorCopies.get(), descriptorCopyEnd)});

		descriptorWriteEnd = 0;
		descriptorCopyEnd = 0;
	}

	void DescriptorUpdateBatch::addImage(vk::DescriptorSet targetDescriptor, u8 targetBinding, vk::ImageView imageView, vk::ImageLayout imageLayout) {
		if (descriptorWriteEnd >= descriptorWriteMax) {
			flush();
		}

		const auto& imageInfo = descriptorInfos[descriptorWriteEnd].emplace<vk::DescriptorImageInfo>(vk::Sampler(), imageView, imageLayout);

		descriptorWrites[descriptorWriteEnd] =
			vk::WriteDescriptorSet(targetDescriptor, targetBinding, 0, 1, vk::DescriptorType::eSampledImage, &imageInfo, nullptr, nullptr);

		++descriptorWriteEnd;
	}

	void DescriptorUpdateBatch::addSampler(vk::DescriptorSet targetDescriptor, u8 targetBinding, vk::Sampler sampler) {
		if (descriptorWriteEnd >= descriptorWriteMax) {
			flush();
		}

		const auto& imageInfo = descriptorInfos[descriptorWriteEnd].emplace<vk::DescriptorImageInfo>(sampler, vk::ImageView(), vk::ImageLayout());

		descriptorWrites[descriptorWriteEnd] =
			vk::WriteDescriptorSet(targetDescriptor, targetBinding, 0, 1, vk::DescriptorType::eSampler, &imageInfo, nullptr, nullptr);

		++descriptorWriteEnd;
	}

	void DescriptorUpdateBatch::addImageSampler(
		vk::DescriptorSet targetDescriptor, u8 targetBinding, vk::ImageView imageView, vk::Sampler sampler, vk::ImageLayout imageLayout
	) {
		if (descriptorWriteEnd >= descriptorWriteMax) {
			flush();
		}

		const auto& imageInfo = descriptorInfos[descriptorWriteEnd].emplace<vk::DescriptorImageInfo>(sampler, imageView, imageLayout);

		descriptorWrites[descriptorWriteEnd] =
			vk::WriteDescriptorSet(targetDescriptor, targetBinding, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfo, nullptr, nullptr);

		++descriptorWriteEnd;
	}

	void DescriptorUpdateBatch::addBuffer(
		vk::DescriptorSet targetDescriptor, u8 targetBinding, vk::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize size
	) {
		if (descriptorWriteEnd >= descriptorWriteMax) {
			flush();
		}

		const auto& bufferInfo = descriptorInfos[descriptorWriteEnd].emplace<vk::DescriptorBufferInfo>(buffer, offset, size);

		descriptorWrites[descriptorWriteEnd] =
			vk::WriteDescriptorSet(targetDescriptor, targetBinding, 0, 1, vk::DescriptorType::eStorageImage, nullptr, &bufferInfo, nullptr);

		++descriptorWriteEnd;
	}

	void DescriptorUpdateBatch::copyBinding(
		vk::DescriptorSet sourceDescriptor, vk::DescriptorSet targetDescriptor, u8 sourceBinding, u8 targetBinding, u8 sourceArrayElement,
		u8 targetArrayElement, u8 descriptorCount
	) {
		if (descriptorCopyEnd >= descriptorCopyMax) {
			flush();
		}

		descriptorCopies[descriptorCopyEnd] = vk::CopyDescriptorSet(
			sourceDescriptor, sourceBinding, sourceArrayElement, targetDescriptor, targetBinding, targetArrayElement, descriptorCount
		);

		++descriptorCopyEnd;
	}

	std::optional<DescriptorUpdateBatch> DescriptorUpdateBatch::create(vk::Device device, usize descriptorWriteMax, usize descriptorCopyMax)

	{
		DescriptorUpdateBatch newDescriptorUpdateBatch(device, descriptorWriteMax, descriptorCopyMax);

		newDescriptorUpdateBatch.descriptorInfos = std::make_unique<DescriptorInfoUnion[]>(descriptorWriteMax);
		newDescriptorUpdateBatch.descriptorWrites = std::make_unique<vk::WriteDescriptorSet[]>(descriptorWriteMax);
		newDescriptorUpdateBatch.descriptorCopies = std::make_unique<vk::CopyDescriptorSet[]>(descriptorCopyMax);

		return {std::move(newDescriptorUpdateBatch)};
	}

}  // namespace Vulkan