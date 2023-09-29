#include "renderer_vk/vk_descriptor_update_batch.hpp"

#include <memory>
#include <span>

namespace Vulkan {

    DescriptorUpdateBatch::DescriptorUpdateBatch(vk::Device device, usize descriptorWriteMax, usize descriptorCopyMax)
        : device(device), descriptorWriteMax(descriptorWriteMax), descriptorCopyMax(descriptorCopyMax) {
        descriptorInfos = std::make_unique<DescriptorInfoUnion[]>(descriptorWriteMax);
        descriptorWrites = std::make_unique<vk::WriteDescriptorSet[]>(descriptorWriteMax);
    }

	void DescriptorUpdateBatch::flush() {
        device.updateDescriptorSets({std::span(descriptorWrites.get(), descriptorWriteEnd)}, {});
		descriptorWriteEnd = 0;
	}

    void DescriptorUpdateBatch::addImage(vk::DescriptorSet targetDescriptor, u8 targetBinding, vk::ImageView imageView, vk::ImageLayout imageLayout) {
		if (descriptorWriteEnd >= descriptorWriteMax) {
			flush();
		}

        const auto& imageInfo = descriptorInfos[descriptorWriteEnd].emplace<vk::DescriptorImageInfo>(vk::Sampler(), imageView, imageLayout);

        descriptorWrites[descriptorWriteEnd++] = vk::WriteDescriptorSet{
            .dstSet = targetDescriptor,
            .dstBinding = targetBinding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eSampledImage,
            .pImageInfo = &imageInfo,
        };
	}

	void DescriptorUpdateBatch::addSampler(vk::DescriptorSet targetDescriptor, u8 targetBinding, vk::Sampler sampler) {
		if (descriptorWriteEnd >= descriptorWriteMax) {
			flush();
		}

		const auto& imageInfo = descriptorInfos[descriptorWriteEnd].emplace<vk::DescriptorImageInfo>(sampler, vk::ImageView(), vk::ImageLayout());

        descriptorWrites[descriptorWriteEnd++] = vk::WriteDescriptorSet{
            .dstSet = targetDescriptor,
            .dstBinding = targetBinding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eSampler,
            .pImageInfo = &imageInfo,
        };
	}

	void DescriptorUpdateBatch::addImageSampler(
        vk::DescriptorSet targetDescriptor, u8 targetBinding, u8 targetArrayIndex, vk::ImageView imageView, vk::Sampler sampler, vk::ImageLayout imageLayout
	) {
		if (descriptorWriteEnd >= descriptorWriteMax) {
			flush();
		}

		const auto& imageInfo = descriptorInfos[descriptorWriteEnd].emplace<vk::DescriptorImageInfo>(sampler, imageView, imageLayout);


        descriptorWrites[descriptorWriteEnd++] = vk::WriteDescriptorSet{
            .dstSet = targetDescriptor,
            .dstBinding = targetBinding,
            .dstArrayElement = targetArrayIndex,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .pImageInfo = &imageInfo,
        };
	}

	void DescriptorUpdateBatch::addBuffer(
		vk::DescriptorSet targetDescriptor, u8 targetBinding, vk::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize size
	) {
		if (descriptorWriteEnd >= descriptorWriteMax) {
			flush();
		}

		const auto& bufferInfo = descriptorInfos[descriptorWriteEnd].emplace<vk::DescriptorBufferInfo>(buffer, offset, size);

        descriptorWrites[descriptorWriteEnd++] = vk::WriteDescriptorSet{
            .dstSet = targetDescriptor,
            .dstBinding = targetBinding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eUniformBufferDynamic,
            .pBufferInfo = &bufferInfo,
        };
	}

}  // namespace Vulkan
