#include "renderer_vk/vk_pipeline_manager.hpp"
#include "PICA/pica_hash.hpp"
#include "PICA/pica_uniforms.hpp"
#include "PICA/pica_vertex.hpp"
#include "renderer_vk/vk_descriptor_heap.hpp"
#include "renderer_vk/vk_scheduler.hpp"
#include "renderer_vk/vk_instance.hpp"

#include <cstddef>
#include <cmrc/cmrc.hpp>

CMRC_DECLARE(RendererVK);

namespace Vulkan {

inline std::array<float, 4> colorRGBA8(const u32 color) {
	const float scale = 1 / 255.f;
	return std::to_array({(color >> 0 & 0xFF) * scale, (color >> 8 & 0xFF) * scale,
						  (color >> 16 & 0xFF) * scale, (color >> 24 & 0xFF) * scale});
}

constexpr std::array<vk::DescriptorSetLayoutBinding, 2> BufferBindings = {{
    {0, vk::DescriptorType::eUniformBufferDynamic, 1, vk::ShaderStageFlagBits::eVertex}, // vsSupportBuffer
    {1, vk::DescriptorType::eUniformBufferDynamic, 1, vk::ShaderStageFlagBits::eFragment}, // fsSupportBuffer
}};

constexpr std::array<vk::DescriptorSetLayoutBinding, 4> TextureBindings = {{
    {0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}, // u_tex0
    {1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}, // u_tex1
    {2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}, // u_tex2
    {3, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}, // u_tex_lighting_lut
}};

constexpr std::array<vk::VertexInputBindingDescription, 1> BindingDescriptions = {{
    {0, sizeof(PICA::Vertex), vk::VertexInputRate::eVertex},
}};

constexpr std::array<vk::VertexInputAttributeDescription, 8> AttributeDescriptions = {{
	{0, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(PICA::Vertex, s.positions)},
	{1, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(PICA::Vertex, s.quaternion)},
	{2, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(PICA::Vertex, s.colour)},
	{3, 0, vk::Format::eR32G32Sfloat, offsetof(PICA::Vertex, s.texcoord0)},
	{4, 0, vk::Format::eR32G32Sfloat, offsetof(PICA::Vertex, s.texcoord1)},
	{5, 0, vk::Format::eR32Sfloat, offsetof(PICA::Vertex, s.texcoord0_w)},
	{6, 0, vk::Format::eR32G32B32Sfloat, offsetof(PICA::Vertex, s.view)},
	{7, 0, vk::Format::eR32G32Sfloat, offsetof(PICA::Vertex, s.texcoord2)},
}};

PipelineManager::PipelineManager(const Instance& instance, Scheduler& scheduler, vk::Buffer streamBuffer)
    : instance(instance), scheduler(scheduler), updateBatch(instance.getDevice()) {
    auto vk_resources = cmrc::RendererVK::get_filesystem();
    auto picaVertexShader = vk_resources.open("vulkan_vertex_shader.vert.spv");
    auto picaFragmentShader = vk_resources.open("vulkan_fragment_shader.frag.spv");

    const vk::Device device = instance.getDevice();
    picaVertexShaderModule = createShaderModule(device, picaVertexShader);
    picaFragmentShaderModule = createShaderModule(device, picaFragmentShader);

	// The buffer descriptor set is immutable, so we only need 1 of it.
	if (auto createResult = Vulkan::DescriptorHeap::create(device, scheduler, BufferBindings, 1); createResult.has_value()) {
		bufferHeap = std::make_unique<Vulkan::DescriptorHeap>(std::move(createResult.value()));
	} else {
		Helpers::panic("Error creating descriptor heap\n");
	}

	// Texture updates happen almost every draw so we need a lot of texture descriptor sets.
	if (auto createResult = Vulkan::DescriptorHeap::create(device, scheduler, TextureBindings); createResult.has_value()) {
		textureHeap = std::make_unique<Vulkan::DescriptorHeap>(std::move(createResult.value()));
	} else {
		Helpers::panic("Error creating descriptor heap\n");
	}

	const std::array setLayouts = {
		bufferHeap.get()->getDescriptorSetLayout(),
		textureHeap.get()->getDescriptorSetLayout(),
	};

	// Create pica pipeline Layout
	const vk::PipelineLayoutCreateInfo layoutInfo = {
		.setLayoutCount = setLayouts.size(),
		.pSetLayouts = setLayouts.data(),
	};

	if (auto createResult = device.createPipelineLayoutUnique(layoutInfo); createResult.result == vk::Result::eSuccess) {
		picaPipelineLayout = std::move(createResult.value);
	} else {
		Helpers::panic("Error creating pipeline layout: %s\n", vk::to_string(createResult.result).c_str());
		return;
	}

	// Allocate and prepare the buffer descriptor set beforehand.
	bufferSet = bufferHeap->allocateDescriptorSet().value();
	updateBatch.addBuffer(bufferSet, 0, streamBuffer, 0, sizeof(VsSupportBuffer));
	updateBatch.addBuffer(bufferSet, 1, streamBuffer, 0, sizeof(FsSupportBuffer));
}

PipelineManager::~PipelineManager() = default;

void PipelineManager::bindPipeline(const PipelineInfo& info) {
	// Retrieve our pipeline from the cache.
	const size_t pipelineHash = PICAHash::computeHash(&info, sizeof(PipelineInfo));
	const auto [it, new_pipeline] = pipelineMap.try_emplace(pipelineHash);
	if (new_pipeline) {
		it->second = std::make_unique<Pipeline>(instance.getDevice(), picaPipelineLayout.get(),
												picaVertexShaderModule.get(), picaFragmentShaderModule.get(),
												BindingDescriptions, AttributeDescriptions, info, info.renderPass);
	}

	// Update dynamic state.
	auto cmdbuf = scheduler.getCmdBuf();
	if (info.dynamic.blendColor != currentInfo.dynamic.blendColor || isDirty) {
		const auto color = colorRGBA8(info.dynamic.blendColor);
		cmdbuf.setBlendConstants(color.data());
	}
	if (info.dynamic.stencilWriteMask != currentInfo.dynamic.stencilWriteMask || isDirty) {
		cmdbuf.setStencilWriteMask(vk::StencilFaceFlagBits::eFrontAndBack, info.dynamic.stencilWriteMask);
	}
	if (info.dynamic.stencilCompareMask != currentInfo.dynamic.stencilCompareMask || isDirty) {
		cmdbuf.setStencilCompareMask(vk::StencilFaceFlagBits::eFrontAndBack, info.dynamic.stencilCompareMask);
	}
	if (info.dynamic.stencilReference != currentInfo.dynamic.stencilReference || isDirty) {
		cmdbuf.setStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, info.dynamic.stencilReference);
	}
	if (info.dynamic.scissor != currentInfo.dynamic.scissor || isDirty) {
		cmdbuf.setScissor(0, info.dynamic.scissor);
	}
	if (info.dynamic.viewport != currentInfo.dynamic.viewport || isDirty) {
		cmdbuf.setViewport(0, info.dynamic.viewport);
	}
	currentInfo = info;
	isDirty = false;

	// Bind descriptor sets and pipeline.
	const std::array descriptorSets = {bufferSet, currentTextureSet};
	cmdbuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, picaPipelineLayout.get(), 0,
							  descriptorSets, dynamicOffsets);
	cmdbuf.bindPipeline(vk::PipelineBindPoint::eGraphics, it->second->getPipeline());
}

}
