#pragma once

#include "helpers.hpp"
#include "PICA/texture.hpp"
#include "renderer_vk/vk_pica.hpp"
#include "renderer_vk/vk_stream_buffer.hpp"

VK_DEFINE_HANDLE(VmaAllocation)

namespace Vulkan {

class Instance;
class Scheduler;

struct Texture : public PICA::Texture {
    const Instance* instance;
    Scheduler* scheduler;
    vk::Format vkFormat;
    vk::Image image;
    VmaAllocation allocation;
    vk::ImageView imageView;

    Texture() = default;
    Texture(u32 loc, PICA::TextureFmt format, u32 x, u32 y, u32 config, bool valid = true)
        : PICA::Texture(loc, format, x, y, config, valid), vkFormat(Vulkan::textureFormatToVulkan(format)) {}
    Texture(vk::Format format, u32 x, u32 y, u32 config, bool valid = true)
        : PICA::Texture(0, PICA::TextureFmt::RGBA8, x, y, config, valid), vkFormat(format) {}

	void allocate(const Instance& instance, Scheduler& scheduler, vk::ImageType imageType = vk::ImageType::e2D,
				  vk::ImageViewType type = vk::ImageViewType::e2D, u32 layers = 1);
    void setNewConfig(u32 newConfig);
    void uploadTexture(std::span<const u8> data, u32 numLayers, StreamBuffer<4096>& upload_buffer);
    void free();
};

struct ColourBuffer : public PICA::ColourBuffer<true> {
    const Instance* instance;
    vk::Format vkFormat;
    vk::Image image;
    VmaAllocation allocation;
    vk::ImageView imageView;

    ColourBuffer() = default;
    ColourBuffer(u32 loc, PICA::ColorFmt format, u32 x, u32 y, bool valid = true)
        : PICA::ColourBuffer<true>(loc, format, x, y, valid), vkFormat(Vulkan::colorFormatToVulkan(format)) {}

    void allocate(const Instance& instance, Scheduler& scheduler);

    void free();
};

struct DepthBuffer : public PICA::DepthBuffer {
    const Instance* instance;
    vk::Format vkFormat;
    vk::Image image;
    VmaAllocation allocation;
    vk::ImageView imageView;

    DepthBuffer() = default;
    DepthBuffer(u32 loc, PICA::DepthFmt format, u32 x, u32 y, bool valid = true)
        : PICA::DepthBuffer(loc, format, x, y, valid), vkFormat(Vulkan::depthFormatToVulkan(format)) {}

    void allocate(const Instance& instance, Scheduler& scheduler);

    void free();
};

} // namespace Vulkan
