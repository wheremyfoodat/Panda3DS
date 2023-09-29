#pragma once

#include <cmrc/cmrc.hpp>
#include "helpers.hpp"
#include "vk_api.hpp"

namespace Vulkan {

class Instance;

struct RasterizationState {
	vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList;
	vk::CullModeFlagBits cullMode = vk::CullModeFlagBits::eNone;
	vk::FrontFace frontFace = vk::FrontFace::eClockwise;
};

struct DepthStencilState {
	u32 depthTestEnable = 0;
	u32 depthWriteEnable = 0;
	u32 stencilTestEnable = 0;
	vk::CompareOp depthCompareOp = vk::CompareOp::eAlways;
	vk::StencilOp stencilFailOp = vk::StencilOp::eKeep;
	vk::StencilOp stencilPassOp = vk::StencilOp::eKeep;
	vk::StencilOp stencilDepthFailOp = vk::StencilOp::eKeep;
	vk::CompareOp stencilCompareOp = vk::CompareOp::eAlways;
};

struct BlendingState {
	u16 blendEnable = 0;
	u16 colorWriteMask = 0xFFFF;
	vk::LogicOp logicOp = vk::LogicOp::eCopy;
	struct {
		vk::BlendFactor srcColorBlendFactor;
		vk::BlendFactor dstColorBlendFactor;
		vk::BlendOp colorBlendEq;
		vk::BlendFactor srcAlphaBlendFactor;
		vk::BlendFactor dstAlphaBlendFactor;
		vk::BlendOp alphaBlendEq;
	};
};

struct DynamicState {
	u32 blendColor = 0;
	u8 stencilReference = 0;
	u8 stencilCompareMask = 0;
	u8 stencilWriteMask = 0;

	vk::Rect2D scissor{};
	vk::Viewport viewport{};

	bool operator==(const DynamicState& other) const noexcept {
		return std::memcmp(this, &other, sizeof(DynamicState)) == 0;
	}
};

struct PipelineInfo {
	BlendingState blending;
	RasterizationState rasterization;
	DepthStencilState depthStencil;
	DynamicState dynamic;
	vk::RenderPass renderPass;

	[[nodiscard]] u64 Hash(const Instance& instance) const;
};
static_assert(std::is_standard_layout_v<PipelineInfo>, "PipelineInfo not suitable for hashing!");

class Pipeline {
    vk::UniquePipeline pipeline{};

  public:
    Pipeline(vk::Device device, vk::PipelineLayout pipelineLayout,
             vk::ShaderModule vertModule, vk::ShaderModule fragModule,
             std::span<const vk::VertexInputBindingDescription> vertexBindingDescriptions,
             std::span<const vk::VertexInputAttributeDescription> vertexAttributeDescriptions,
             const PipelineInfo& info,
             vk::RenderPass renderPass);
    ~Pipeline();

    vk::Pipeline getPipeline() const noexcept {
        return *pipeline;
    }
};

inline vk::UniqueShaderModule createShaderModule(vk::Device device, std::span<const std::byte> shaderCode) {
	vk::ShaderModuleCreateInfo shaderModuleInfo = {};
	shaderModuleInfo.pCode = reinterpret_cast<const u32*>(shaderCode.data());
	shaderModuleInfo.codeSize = shaderCode.size();

	vk::UniqueShaderModule shaderModule = {};
	if (auto createResult = device.createShaderModuleUnique(shaderModuleInfo); createResult.result == vk::Result::eSuccess) {
		shaderModule = std::move(createResult.value);
	} else {
		Helpers::panic("Error creating shader module: %s\n", vk::to_string(createResult.result).c_str());
	}
	return shaderModule;
}

inline vk::UniqueShaderModule createShaderModule(vk::Device device, cmrc::file shaderFile) {
	return createShaderModule(device, std::span<const std::byte>(reinterpret_cast<const std::byte*>(shaderFile.begin()), shaderFile.size()));
}

} // namespace Vulkan
