#include "renderer_vk/vk_pica.hpp"

namespace Vulkan {

	vk::Format colorFormatToVulkan(PICA::ColorFmt colorFormat) {
		switch (colorFormat) {
			case PICA::ColorFmt::RGBA8: return vk::Format::eR8G8B8A8Unorm;
			case PICA::ColorFmt::RGB8: return vk::Format::eR8G8B8Unorm;
			case PICA::ColorFmt::RGBA5551: return vk::Format::eR5G5B5A1UnormPack16;
			case PICA::ColorFmt::RGB565: return vk::Format::eR5G6B5UnormPack16;
			case PICA::ColorFmt::RGBA4: return vk::Format::eR4G4B4A4UnormPack16;
		}
		return vk::Format::eUndefined;
	}
	vk::Format depthFormatToVulkan(PICA::DepthFmt depthFormat) {
		switch (depthFormat) {
			case PICA::DepthFmt::Depth16: return vk::Format::eD16Unorm;
			case PICA::DepthFmt::Unknown1: return vk::Format::eUndefined;
			case PICA::DepthFmt::Depth24: return vk::Format::eX8D24UnormPack32;
			case PICA::DepthFmt::Depth24Stencil8: return vk::Format::eD24UnormS8Uint;
		}
		return vk::Format::eUndefined;
	}

}  // namespace Vulkan