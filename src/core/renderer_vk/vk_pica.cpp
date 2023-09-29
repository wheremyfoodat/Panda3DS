#include "renderer_vk/vk_pica.hpp"

namespace Vulkan {

	vk::Format colorFormatToVulkan(PICA::ColorFmt colorFormat) {
		switch (colorFormat) {
			case PICA::ColorFmt::RGBA8: return vk::Format::eR8G8B8A8Unorm;
			// VK_FORMAT_R8G8B8A8_UNORM is mandated by the vulkan specification
			// VK_FORMAT_R8G8B8_UNORM may not be supported
			// TODO: Detect this!
			// case PICA::ColorFmt::RGB8: return vk::Format::eR8G8B8Unorm;
			case PICA::ColorFmt::RGB8: return vk::Format::eR8G8B8A8Unorm;
			case PICA::ColorFmt::RGBA5551: return vk::Format::eR8G8B8A8Unorm;
			case PICA::ColorFmt::RGB565: return vk::Format::eR8G8B8A8Unorm;
			case PICA::ColorFmt::RGBA4: return vk::Format::eR8G8B8A8Unorm;
            default: return vk::Format::eUndefined;
		}
	}
    vk::Format textureFormatToVulkan(PICA::TextureFmt textureFormat) {
        switch (textureFormat) {
            case PICA::TextureFmt::RGBA8: return vk::Format::eR8G8B8A8Unorm;
            // VK_FORMAT_R8G8B8A8_UNORM is mandated by the vulkan specification
            // VK_FORMAT_R8G8B8_UNORM may not be supported
            // TODO: Detect this!
            // case PICA::TextureFmt::RGB8: return vk::Format::eR8G8B8Unorm;
            case PICA::TextureFmt::RGB8: return vk::Format::eR8G8B8A8Unorm;
            case PICA::TextureFmt::RGBA5551: return vk::Format::eR8G8B8A8Unorm;
            case PICA::TextureFmt::RGB565: return vk::Format::eR8G8B8A8Unorm;
            case PICA::TextureFmt::RGBA4: return vk::Format::eR8G8B8A8Unorm;
            case PICA::TextureFmt::IA8:
            case PICA::TextureFmt::RG8:
            case PICA::TextureFmt::I8:
            case PICA::TextureFmt::A8:
            case PICA::TextureFmt::IA4:
            case PICA::TextureFmt::I4:
            case PICA::TextureFmt::A4:
            case PICA::TextureFmt::ETC1:
            case PICA::TextureFmt::ETC1A4: return vk::Format::eR8G8B8A8Unorm;
            default: return vk::Format::eUndefined;
        }
    }
	vk::Format depthFormatToVulkan(PICA::DepthFmt depthFormat) {
		switch (depthFormat) {
			// VK_FORMAT_D16_UNORM is mandated by the vulkan specification
			case PICA::DepthFmt::Depth16: return vk::Format::eD16Unorm;
			case PICA::DepthFmt::Unknown1: return vk::Format::eUndefined;
			// The GPU may _not_ support these formats natively
			// Only one of:
			// VK_FORMAT_X8_D24_UNORM_PACK32 and VK_FORMAT_D32_SFLOAT
			// and one of:
			// VK_FORMAT_D24_UNORM_S8_UINT and VK_FORMAT_D32_SFLOAT_S8_UINT
			// will be supported
			// TODO: Detect this!
			// case PICA::DepthFmt::Depth24: return vk::Format::eX8D24UnormPack32;
			// case PICA::DepthFmt::Depth24Stencil8: return vk::Format::eD24UnormS8Uint;
			case PICA::DepthFmt::Depth24: return vk::Format::eD32Sfloat;
			case PICA::DepthFmt::Depth24Stencil8: return vk::Format::eD32SfloatS8Uint;
            default: return vk::Format::eUndefined;
		}
	}
    vk::ImageAspectFlags formatAspect(vk::Format format) {
        switch (format) {
            case vk::Format::eD16Unorm:
            case vk::Format::eD32Sfloat: return vk::ImageAspectFlagBits::eDepth;
            case vk::Format::eD32SfloatS8Uint: return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
            default: return vk::ImageAspectFlagBits::eColor;
        }
    }


}  // namespace Vulkan
