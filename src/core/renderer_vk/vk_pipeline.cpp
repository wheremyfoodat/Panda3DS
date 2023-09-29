#include "helpers.hpp"
#include "renderer_vk/vk_pipeline.hpp"

namespace Vulkan {

Pipeline::Pipeline(vk::Device device, vk::PipelineLayout pipelineLayout,
               vk::ShaderModule vertModule, vk::ShaderModule fragModule,
               std::span<const vk::VertexInputBindingDescription> vertexBindingDescriptions,
               std::span<const vk::VertexInputAttributeDescription> vertexAttributeDescriptions,
               const PipelineInfo& info,
               vk::RenderPass renderPass) {
    // Describe the stage and entry point of each shader
    const std::array shaderStagesInfo = {
        vk::PipelineShaderStageCreateInfo{
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = vertModule,
            .pName = "main",
        },
        vk::PipelineShaderStageCreateInfo{
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = fragModule,
            .pName = "main",
        },
    };

    const vk::PipelineVertexInputStateCreateInfo vertexInputState = {
        .vertexBindingDescriptionCount = static_cast<u32>(vertexBindingDescriptions.size()),
        .pVertexBindingDescriptions = vertexBindingDescriptions.data(),
        .vertexAttributeDescriptionCount = static_cast<u32>(vertexAttributeDescriptions.size()),
        .pVertexAttributeDescriptions = vertexAttributeDescriptions.data(),
    };

    const vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState = {
        .topology = info.rasterization.topology,
        .primitiveRestartEnable = false,
    };

    static const vk::Viewport defaultViewport = {0, 0, 16, 16, 0.0f, 1.0f};
    static const vk::Rect2D defaultScissor = {{0, 0}, {16, 16}};

    const vk::PipelineViewportStateCreateInfo viewportState = {
        .viewportCount = 1,
        .pViewports = &defaultViewport,
        .scissorCount = 1,
        .pScissors = &defaultScissor,
    };

    const vk::PipelineRasterizationStateCreateInfo rasterizationState = {
        .depthClampEnable = false,
        .rasterizerDiscardEnable = false,
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = info.rasterization.cullMode,
        .frontFace = info.rasterization.frontFace,
        .depthBiasEnable = false,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 0.0,
        .lineWidth = 1.0f,
    };

    const vk::PipelineMultisampleStateCreateInfo multisampleState = {
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable = false,
        .minSampleShading = 1.0f,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = true,
        .alphaToOneEnable = false,
    };

	const vk::StencilOpState stencilOpState = {
		.failOp = info.depthStencil.stencilFailOp,
		.passOp = info.depthStencil.stencilPassOp,
		.depthFailOp = info.depthStencil.stencilDepthFailOp,
		.compareOp = info.depthStencil.stencilCompareOp,
	};

    const vk::PipelineDepthStencilStateCreateInfo depthStencilState = {
        .depthTestEnable = info.depthStencil.depthTestEnable,
        .depthWriteEnable = info.depthStencil.depthWriteEnable,
        .depthCompareOp = info.depthStencil.depthCompareOp,
        .depthBoundsTestEnable = false,
        .stencilTestEnable = info.depthStencil.stencilTestEnable,
        .front = stencilOpState,
        .back = stencilOpState,
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
    };

    const vk::PipelineColorBlendAttachmentState blendAttachmentState = {
        .blendEnable = info.blending.blendEnable,
        .srcColorBlendFactor = info.blending.srcColorBlendFactor,
        .dstColorBlendFactor = info.blending.dstColorBlendFactor,
        .colorBlendOp = info.blending.colorBlendEq,
        .srcAlphaBlendFactor = info.blending.srcAlphaBlendFactor,
        .dstAlphaBlendFactor = info.blending.dstAlphaBlendFactor,
        .alphaBlendOp = info.blending.alphaBlendEq,
        .colorWriteMask = static_cast<vk::ColorComponentFlags>(info.blending.colorWriteMask),
    };

    const vk::PipelineColorBlendStateCreateInfo colorBlendState = {
        .logicOpEnable = !info.blending.blendEnable,
        .logicOp = info.blending.logicOp,
        .attachmentCount = 1,
        .pAttachments = &blendAttachmentState,
    };

    // TODO: Utilize more dynamic states available on modern GPUs
    static constexpr std::array dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
        vk::DynamicState::eBlendConstants,
        vk::DynamicState::eStencilCompareMask,
        vk::DynamicState::eStencilWriteMask,
        vk::DynamicState::eStencilReference,
    };

    const vk::PipelineDynamicStateCreateInfo dynamicState = {
        .dynamicStateCount = static_cast<u32>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data(),
    };

    const vk::GraphicsPipelineCreateInfo renderPipelineInfo = {
        .stageCount = 2,
        .pStages = shaderStagesInfo.data(),
        .pVertexInputState = &vertexInputState,
        .pInputAssemblyState = &inputAssemblyState,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizationState,
        .pMultisampleState = &multisampleState,
        .pDepthStencilState = &depthStencilState,
        .pColorBlendState = &colorBlendState,
        .pDynamicState = &dynamicState,
        .layout = pipelineLayout,
        .renderPass = renderPass,
    };

    // Create Pipeline
    if (auto createResult = device.createGraphicsPipelineUnique({}, renderPipelineInfo); createResult.result == vk::Result::eSuccess) {
        pipeline = std::move(createResult.value);
    } else {
        Helpers::panic("Error creating graphics pipeline: %s\n", vk::to_string(createResult.result).c_str());
    }
}

Pipeline::~Pipeline() = default;

} // namespace Vulkan
