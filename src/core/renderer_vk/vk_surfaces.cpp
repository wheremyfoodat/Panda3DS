#include "renderer_vk/vk_surfaces.hpp"
#include "renderer_vk/vk_pica.hpp"
#include "renderer_vk/vk_debug.hpp"
#include "renderer_vk/vk_instance.hpp"
#include "renderer_vk/vk_scheduler.hpp"

#include <vk_mem_alloc.h>

namespace Vulkan {

template <typename T>
[[nodiscard]] constexpr T alignUp(T value, std::size_t size) {
	static_assert(std::is_unsigned_v<T>, "T must be an unsigned value.");
	auto mod{static_cast<T>(value % size)};
	value -= mod;
	return static_cast<T>(mod == T{0} ? value : value + size);
}

static std::pair<vk::Image, VmaAllocation> allocateImage(const Instance& instance, vk::Format format, Math::uvec2 size, u32 layers,
                                                         u32 location, vk::ImageUsageFlagBits usage, vk::ImageType imageType = vk::ImageType::e2D) {
    const vk::ImageCreateInfo textureInfo = {
        .imageType = imageType,
        .format = format,
        .extent = {
            .width = size.x(),
            .height = size.y(),
            .depth = 1,
        },
        .mipLevels = 1,
        .arrayLayers = layers,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = vk::ImageTiling::eOptimal,
        .usage = usage | vk::ImageUsageFlagBits::eTransferSrc |
                 vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        .sharingMode = vk::SharingMode::eExclusive,
        .initialLayout = vk::ImageLayout::eUndefined,
    };

    const VmaAllocationCreateInfo allocInfo = {
        .flags = VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .requiredFlags = 0,
        .preferredFlags = 0,
        .pool = VK_NULL_HANDLE,
        .pUserData = nullptr,
    };

    const VkImageCreateInfo vmaInfo = static_cast<VkImageCreateInfo>(textureInfo);
    VmaAllocation allocation{};
    VkImage vmaImage;

    const VkResult result = vmaCreateImage(instance.getAllocator(), &vmaInfo, &allocInfo,
                                           &vmaImage, &allocation, nullptr);
    if (result != VK_SUCCESS) [[unlikely]] {
        Helpers::panic("Failed allocating image with error %d\n", result);
        return {};
    }

    const vk::Image image{vmaImage};
    Vulkan::setObjectName(
        instance.getDevice(), image, "TextureCache:%08x %ux%u %s",
        location, size.x(), size.y(), vk::to_string(textureInfo.format).c_str()
    );

    return std::make_pair(image, allocation);
}

static vk::ImageView allocateImageView(const Instance& instance, vk::Format format, vk::Image image, vk::ImageViewType type) {
    const vk::ImageViewCreateInfo viewInfo = {
        .image = image,
        .viewType = type,
        .format = format,
        .components = vk::ComponentMapping(),
        .subresourceRange = {
            .aspectMask = Vulkan::formatAspect(format),
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    if (auto createResult = instance.getDevice().createImageView(viewInfo); createResult.result == vk::Result::eSuccess) {
        return createResult.value;
    } else {
        Helpers::panic("Error creating color render-texture: %s\n", vk::to_string(createResult.result).c_str());
        return {};
    }
}

static void recordInitBarrier(vk::CommandBuffer cmdbuf, vk::Image image, vk::Format format) {
    const vk::ImageMemoryBarrier initBarrier = {
        .srcAccessMask = vk::AccessFlagBits::eNone,
        .dstAccessMask = vk::AccessFlagBits::eNone,
        .oldLayout = vk::ImageLayout::eUndefined,
        .newLayout = vk::ImageLayout::eGeneral,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = Vulkan::formatAspect(format),
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        },
    };
    cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eBottomOfPipe,
                           vk::DependencyFlagBits::eByRegion, {}, {}, initBarrier);
}

// TODO: Deduplicate following code when surface caches are merged

void Texture::allocate(const Instance& instance_, Scheduler& scheduler_, vk::ImageType imageType, vk::ImageViewType type, u32 layers) {
    instance = &instance_;
    scheduler = &scheduler_;
    std::tie(image, allocation) = allocateImage(instance_, vkFormat, size, layers, location, vk::ImageUsageFlagBits::eColorAttachment, imageType);
    imageView = allocateImageView(instance_, vkFormat, image, type);
    recordInitBarrier(scheduler->getCmdBuf(), image, vkFormat);
}

void Texture::free() {
    valid = false;
    /*if (scheduler) {
        scheduler->finish();
    }
    if (image && instance) {
        vmaDestroyImage(instance->getAllocator(), image, allocation);
    }
    if (imageView && instance) {
        instance->getDevice().destroyImageView(imageView);
    }*/
}

void Texture::uploadTexture(std::span<const u8> data, u32 numLayers, StreamBuffer<4096>& uploadBuffer) {
    const u32 mapSize = alignUp(data.size(), 4096);
    const auto [bufferData, offset] = uploadBuffer.Map(mapSize);
    std::memcpy(bufferData.data(), data.data(), data.size());

	const vk::BufferImageCopy uploadCopy = {
		.bufferOffset = offset,
		.bufferRowLength = size.x(),
		.bufferImageHeight = size.y(),
		.imageSubresource{
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = numLayers,
		},
		.imageOffset = {0, 0, 0},
		.imageExtent = {size.x(), size.y(), 1},
	};
	scheduler->getCmdBuf().copyBufferToImage(uploadBuffer.getHandle(), image, vk::ImageLayout::eGeneral, uploadCopy);
}

void ColourBuffer::allocate(const Instance& instance_, Scheduler& scheduler) {
    instance = &instance_;
    std::tie(image, allocation) = allocateImage(instance_, vkFormat, size, 1, location, vk::ImageUsageFlagBits::eColorAttachment);
    imageView = allocateImageView(instance_, vkFormat, image, vk::ImageViewType::e2D);
    recordInitBarrier(scheduler.getCmdBuf(), image, vkFormat);
}

void ColourBuffer::free() {
    valid = false;
    if (image && instance) {
        vmaDestroyImage(instance->getAllocator(), image, allocation);
    }
    if (imageView && instance) {
        instance->getDevice().destroyImageView(imageView);
    }
}

void DepthBuffer::allocate(const Instance& instance_, Scheduler& scheduler) {
    instance = &instance_;
    std::tie(image, allocation) = allocateImage(instance_, vkFormat, size, 1, location, vk::ImageUsageFlagBits::eDepthStencilAttachment);
    imageView = allocateImageView(instance_, vkFormat, image, vk::ImageViewType::e2D);
    recordInitBarrier(scheduler.getCmdBuf(), image, vkFormat);
}

void DepthBuffer::free() {
    valid = false;
    if (image && instance) {
        vmaDestroyImage(instance->getAllocator(), image, allocation);
    }
    if (imageView && instance) {
        instance->getDevice().destroyImageView(imageView);
    }
}

} // namespace Vulkan
