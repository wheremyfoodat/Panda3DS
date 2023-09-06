#pragma once

#include "PICA/gpu.hpp"
#include "helpers.hpp"
#include "vk_api.hpp"

namespace Vulkan {

	vk::Format colorFormatToVulkan(PICA::ColorFmt colorFormat);
	vk::Format depthFormatToVulkan(PICA::DepthFmt depthFormat);

}  // namespace Vulkan