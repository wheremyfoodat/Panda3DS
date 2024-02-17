#pragma once

#include "PICA/gpu.hpp"
#include "vk_api.hpp"

namespace Vulkan {

	vk::Format colorFormatToVulkan(PICA::ColorFmt colorFormat);
	vk::Format depthFormatToVulkan(PICA::DepthFmt depthFormat);
    vk::Format textureFormatToVulkan(PICA::TextureFmt textureFormat);
    vk::ImageAspectFlags formatAspect(vk::Format format);

}  // namespace Vulkan
