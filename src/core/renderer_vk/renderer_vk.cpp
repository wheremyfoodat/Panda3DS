#include "renderer_vk/renderer_vk.hpp"

#include <cmrc/cmrc.hpp>
#include <limits>
#include <span>
#include <unordered_set>

#include "SDL_vulkan.h"
#include "helpers.hpp"
#include "renderer_vk/vk_debug.hpp"
#include "renderer_vk/vk_memory.hpp"
#include "renderer_vk/vk_pica.hpp"

CMRC_DECLARE(RendererVK);

static vk::SamplerCreateInfo sampler2D(bool filtered = true, bool clamp = false) {
	vk::SamplerCreateInfo samplerInfo = {};
	samplerInfo.magFilter = filtered ? vk::Filter::eLinear : vk::Filter::eNearest;
	samplerInfo.minFilter = filtered ? vk::Filter::eLinear : vk::Filter::eNearest;

	samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;

	samplerInfo.addressModeU = clamp ? vk::SamplerAddressMode::eClampToEdge : vk::SamplerAddressMode::eRepeat;
	samplerInfo.addressModeV = clamp ? vk::SamplerAddressMode::eClampToEdge : vk::SamplerAddressMode::eRepeat;
	samplerInfo.addressModeW = clamp ? vk::SamplerAddressMode::eClampToEdge : vk::SamplerAddressMode::eRepeat;

	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.maxAnisotropy = 16.0f;

	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = vk::CompareOp::eAlways;

	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
	samplerInfo.borderColor = vk::BorderColor::eFloatTransparentBlack;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	return samplerInfo;
}

static vk::UniqueShaderModule createShaderModule(vk::Device device, std::span<const std::byte> shaderCode) {
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

static inline vk::UniqueShaderModule createShaderModule(vk::Device device, cmrc::file shaderFile) {
	return createShaderModule(device, std::span<const std::byte>(reinterpret_cast<const std::byte*>(shaderFile.begin()), shaderFile.size()));
}

std::tuple<vk::UniquePipeline, vk::UniquePipelineLayout> createGraphicsPipeline(
	vk::Device device, std::span<const vk::PushConstantRange> pushConstants, std::span<const vk::DescriptorSetLayout> setLayouts,
	vk::ShaderModule vertModule, vk::ShaderModule fragModule, std::span<const vk::VertexInputBindingDescription> vertexBindingDescriptions,
	std::span<const vk::VertexInputAttributeDescription> vertexAttributeDescriptions, vk::RenderPass renderPass
) {
	// Create Pipeline Layout
	vk::PipelineLayoutCreateInfo graphicsPipelineLayoutInfo = {};

	graphicsPipelineLayoutInfo.pSetLayouts = setLayouts.data();
	graphicsPipelineLayoutInfo.setLayoutCount = setLayouts.size();
	graphicsPipelineLayoutInfo.pPushConstantRanges = pushConstants.data();
	graphicsPipelineLayoutInfo.pushConstantRangeCount = pushConstants.size();

	vk::UniquePipelineLayout graphicsPipelineLayout = {};
	if (auto createResult = device.createPipelineLayoutUnique(graphicsPipelineLayoutInfo); createResult.result == vk::Result::eSuccess) {
		graphicsPipelineLayout = std::move(createResult.value);
	} else {
		Helpers::panic("Error creating pipeline layout: %s\n", vk::to_string(createResult.result).c_str());
		return {};
	}

	// Describe the stage and entry point of each shader
	const vk::PipelineShaderStageCreateInfo ShaderStagesInfo[2] = {
		vk::PipelineShaderStageCreateInfo(
			{},                                // Flags
			vk::ShaderStageFlagBits::eVertex,  // Shader Stage
			vertModule,                        // Shader Module
			"main",                            // Shader entry point function name
			{}                                 // Shader specialization info
		),
		vk::PipelineShaderStageCreateInfo(
			{},                                  // Flags
			vk::ShaderStageFlagBits::eFragment,  // Shader Stage
			fragModule,                          // Shader Module
			"main",                              // Shader entry point function name
			{}                                   // Shader specialization info
		),
	};

	vk::PipelineVertexInputStateCreateInfo vertexInputState = {};

	vertexInputState.vertexBindingDescriptionCount = vertexBindingDescriptions.size();
	vertexInputState.pVertexBindingDescriptions = vertexBindingDescriptions.data();

	vertexInputState.vertexAttributeDescriptionCount = vertexAttributeDescriptions.size();
	vertexInputState.pVertexAttributeDescriptions = vertexAttributeDescriptions.data();

	vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
	inputAssemblyState.topology = vk::PrimitiveTopology::eTriangleList;
	inputAssemblyState.primitiveRestartEnable = false;

	vk::PipelineViewportStateCreateInfo viewportState = {};

	static const vk::Viewport defaultViewport = {0, 0, 16, 16, 0.0f, 1.0f};
	static const vk::Rect2D defaultScissor = {{0, 0}, {16, 16}};
	viewportState.viewportCount = 1;
	viewportState.pViewports = &defaultViewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &defaultScissor;

	vk::PipelineRasterizationStateCreateInfo rasterizationState = {};

	rasterizationState.depthClampEnable = false;
	rasterizationState.rasterizerDiscardEnable = false;
	rasterizationState.polygonMode = vk::PolygonMode::eFill;
	rasterizationState.cullMode = vk::CullModeFlagBits::eBack;
	rasterizationState.frontFace = vk::FrontFace::eClockwise;
	rasterizationState.depthBiasEnable = false;
	rasterizationState.depthBiasConstantFactor = 0.0f;
	rasterizationState.depthBiasClamp = 0.0f;
	rasterizationState.depthBiasSlopeFactor = 0.0;
	rasterizationState.lineWidth = 1.0f;

	vk::PipelineMultisampleStateCreateInfo multisampleState = {};

	multisampleState.rasterizationSamples = vk::SampleCountFlagBits::e1;
	multisampleState.sampleShadingEnable = false;
	multisampleState.minSampleShading = 1.0f;
	multisampleState.pSampleMask = nullptr;
	multisampleState.alphaToCoverageEnable = true;
	multisampleState.alphaToOneEnable = false;

	vk::PipelineDepthStencilStateCreateInfo depthStencilState = {};

	depthStencilState.depthTestEnable = false;
	depthStencilState.depthWriteEnable = false;
	depthStencilState.depthCompareOp = vk::CompareOp::eLessOrEqual;
	depthStencilState.depthBoundsTestEnable = false;
	depthStencilState.stencilTestEnable = false;
	depthStencilState.front = vk::StencilOp::eKeep;
	depthStencilState.back = vk::StencilOp::eKeep;
	depthStencilState.minDepthBounds = 0.0f;
	depthStencilState.maxDepthBounds = 1.0f;

	vk::PipelineColorBlendStateCreateInfo colorBlendState = {};

	colorBlendState.logicOpEnable = false;
	colorBlendState.logicOp = vk::LogicOp::eClear;
	colorBlendState.attachmentCount = 1;

	vk::PipelineColorBlendAttachmentState blendAttachmentState = {};

	blendAttachmentState.blendEnable = false;
	blendAttachmentState.srcColorBlendFactor = vk::BlendFactor::eZero;
	blendAttachmentState.dstColorBlendFactor = vk::BlendFactor::eZero;
	blendAttachmentState.colorBlendOp = vk::BlendOp::eAdd;
	blendAttachmentState.srcAlphaBlendFactor = vk::BlendFactor::eZero;
	blendAttachmentState.dstAlphaBlendFactor = vk::BlendFactor::eZero;
	blendAttachmentState.alphaBlendOp = vk::BlendOp::eAdd;
	blendAttachmentState.colorWriteMask =
		vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

	colorBlendState.pAttachments = &blendAttachmentState;

	vk::PipelineDynamicStateCreateInfo dynamicState = {};
	static vk::DynamicState dynamicStates[] = {// The viewport and scissor of the framebuffer will be dynamic at
											   // run-time
											   vk::DynamicState::eViewport, vk::DynamicState::eScissor};
	dynamicState.dynamicStateCount = std::size(dynamicStates);
	dynamicState.pDynamicStates = dynamicStates;

	vk::GraphicsPipelineCreateInfo renderPipelineInfo = {};

	renderPipelineInfo.stageCount = 2;  // Vert + Frag stages
	renderPipelineInfo.pStages = ShaderStagesInfo;
	renderPipelineInfo.pVertexInputState = &vertexInputState;
	renderPipelineInfo.pInputAssemblyState = &inputAssemblyState;
	renderPipelineInfo.pViewportState = &viewportState;
	renderPipelineInfo.pRasterizationState = &rasterizationState;
	renderPipelineInfo.pMultisampleState = &multisampleState;
	renderPipelineInfo.pDepthStencilState = &depthStencilState;
	renderPipelineInfo.pColorBlendState = &colorBlendState;
	renderPipelineInfo.pDynamicState = &dynamicState;
	renderPipelineInfo.subpass = 0;
	renderPipelineInfo.renderPass = renderPass;
	renderPipelineInfo.layout = graphicsPipelineLayout.get();

	// Create Pipeline
	vk::UniquePipeline pipeline = {};

	if (auto createResult = device.createGraphicsPipelineUnique({}, renderPipelineInfo); createResult.result == vk::Result::eSuccess) {
		pipeline = std::move(createResult.value);
	} else {
		Helpers::panic("Error creating graphics pipeline: %s\n", vk::to_string(createResult.result).c_str());
		return {};
	}

	return std::make_tuple(std::move(pipeline), std::move(graphicsPipelineLayout));
}

// Finds the first queue family that satisfies `queueMask` and excludes `queueExcludeMask` bits
// Returns -1 if not found
// Todo: Smarter selection for present/graphics/compute/transfer
static s32 findQueueFamily(
	std::span<const vk::QueueFamilyProperties> queueFamilies, vk::QueueFlags queueMask,
	vk::QueueFlags queueExcludeMask = vk::QueueFlagBits::eProtected
) {
	for (usize i = 0; i < queueFamilies.size(); ++i) {
		if (((queueFamilies[i].queueFlags & queueMask) == queueMask) && !(queueFamilies[i].queueFlags & queueExcludeMask)) {
			return i;
		}
	}
	return -1;
}

static u32 rotl32(u32 x, u32 n) { return (x << n) | (x >> (32 - n)); }
static u32 ror32(u32 x, u32 n) { return (x >> n) | (x << (32 - n)); }

// Lower 32 bits is the format + size, upper 32-bits is the address
static u64 colorBufferHash(u32 loc, u32 size, PICA::ColorFmt format) {
	return (static_cast<u64>(loc) << 32) | (ror32(size, 23) ^ static_cast<u32>(format));
}
static u64 depthBufferHash(u32 loc, u32 size, PICA::DepthFmt format) {
	return (static_cast<u64>(loc) << 32) | (ror32(size, 29) ^ static_cast<u32>(format));
}

RendererVK::Texture* RendererVK::findRenderTexture(u32 addr) {
	// Find first render-texture hash that is >= to addr
	auto match = textureCache.lower_bound(static_cast<u64>(addr) << 32);

	if (match == textureCache.end()) {
		// Not found
		return nullptr;
	}

	Texture* texture = &match->second;

	const usize sizeInBytes = texture->size[0] * texture->size[1] * texture->sizePerPixel;

	// Ensure this address is within the span of the texture
	if ((addr - match->second.loc) <= sizeInBytes) {
		return texture;
	}

	return nullptr;
}

RendererVK::Texture& RendererVK::getColorRenderTexture(u32 addr, PICA::ColorFmt format, u32 width, u32 height) {
	const u64 renderTextureHash = colorBufferHash(addr, width * height * PICA::sizePerPixel(format), format);

	// Cache hit
	if (textureCache.contains(renderTextureHash)) {
		return textureCache.at(renderTextureHash);
	}

	// Cache miss
	Texture& newTexture = textureCache[renderTextureHash];
	newTexture.loc = addr;
	newTexture.sizePerPixel = PICA::sizePerPixel(format);
	newTexture.size = fbSize;

	vk::ImageCreateInfo textureInfo = {};
	textureInfo.setImageType(vk::ImageType::e2D);
	textureInfo.setFormat(Vulkan::colorFormatToVulkan(format));
	textureInfo.setExtent(vk::Extent3D(width, height, 1));
	textureInfo.setMipLevels(1);
	textureInfo.setArrayLayers(1);
	textureInfo.setSamples(vk::SampleCountFlagBits::e1);
	textureInfo.setTiling(vk::ImageTiling::eOptimal);
	textureInfo.setUsage(
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment | vk::ImageUsageFlagBits::eTransferSrc |
		vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled
	);
	textureInfo.setSharingMode(vk::SharingMode::eExclusive);
	textureInfo.setInitialLayout(vk::ImageLayout::eUndefined);

	if (auto createResult = device->createImageUnique(textureInfo); createResult.result == vk::Result::eSuccess) {
		newTexture.image = std::move(createResult.value);
	} else {
		Helpers::panic("Error creating color render-texture image: %s\n", vk::to_string(createResult.result).c_str());
	}

	Vulkan::setObjectName(
		device.get(), newTexture.image.get(), "TextureCache:%08x %ux%u %s", addr, width, height, vk::to_string(textureInfo.format).c_str()
	);

	vk::ImageViewCreateInfo viewInfo = {};
	viewInfo.image = newTexture.image.get();
	viewInfo.viewType = vk::ImageViewType::e2D;
	viewInfo.format = textureInfo.format;
	viewInfo.components = vk::ComponentMapping();
	viewInfo.subresourceRange = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

	if (auto [result, imageMemory] = Vulkan::commitImageHeap(device.get(), physicalDevice, {&newTexture.image.get(), 1});
		result == vk::Result::eSuccess) {
		newTexture.imageMemory = std::move(imageMemory);
	} else {
		Helpers::panic("Error allocating color render-texture memory: %s\n", vk::to_string(result).c_str());
	}

	if (auto createResult = device->createImageViewUnique(viewInfo); createResult.result == vk::Result::eSuccess) {
		newTexture.imageView = std::move(createResult.value);
	} else {
		Helpers::panic("Error creating color render-texture: %s\n", vk::to_string(createResult.result).c_str());
	}

	// Initial layout transition
	getCurrentCommandBuffer().pipelineBarrier(
		vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlags{}, {}, {},
		{vk::ImageMemoryBarrier(
			vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eShaderRead, vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, newTexture.image.get(), viewInfo.subresourceRange
		)}
	);

	return newTexture;
}

RendererVK::Texture& RendererVK::getDepthRenderTexture(u32 addr, PICA::DepthFmt format, u32 width, u32 height) {
	const u64 renderTextureHash = depthBufferHash(addr, width * height * PICA::sizePerPixel(format), format);

	// Cache hit
	if (textureCache.contains(renderTextureHash)) {
		return textureCache.at(renderTextureHash);
	}

	// Cache miss
	Texture& newTexture = textureCache[renderTextureHash];
	newTexture.loc = addr;
	newTexture.sizePerPixel = PICA::sizePerPixel(format);
	newTexture.size = fbSize;

	vk::ImageCreateInfo textureInfo = {};
	textureInfo.setImageType(vk::ImageType::e2D);
	textureInfo.setFormat(Vulkan::depthFormatToVulkan(format));
	textureInfo.setExtent(vk::Extent3D(width, height, 1));
	textureInfo.setMipLevels(1);
	textureInfo.setArrayLayers(1);
	textureInfo.setSamples(vk::SampleCountFlagBits::e1);
	textureInfo.setTiling(vk::ImageTiling::eOptimal);
	textureInfo.setUsage(
		vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eInputAttachment | vk::ImageUsageFlagBits::eTransferSrc |
		vk::ImageUsageFlagBits::eTransferDst
	);
	textureInfo.setSharingMode(vk::SharingMode::eExclusive);
	textureInfo.setInitialLayout(vk::ImageLayout::eUndefined);

	if (auto createResult = device->createImageUnique(textureInfo); createResult.result == vk::Result::eSuccess) {
		newTexture.image = std::move(createResult.value);
	} else {
		Helpers::panic("Error creating depth render-texture image: %s\n", vk::to_string(createResult.result).c_str());
	}

	Vulkan::setObjectName(
		device.get(), newTexture.image.get(), "TextureCache:%08x %ux%u %s", addr, width, height, vk::to_string(textureInfo.format).c_str()
	);

	vk::ImageViewCreateInfo viewInfo = {};
	viewInfo.image = newTexture.image.get();
	viewInfo.viewType = vk::ImageViewType::e2D;
	viewInfo.format = textureInfo.format;
	viewInfo.components = vk::ComponentMapping();
	// viewInfo.subresourceRange = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil, 0, 1, 0, 1);
	viewInfo.subresourceRange = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1);

	if (PICA::hasStencil(format)) {
		viewInfo.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
	}

	if (auto [result, imageMemory] = Vulkan::commitImageHeap(device.get(), physicalDevice, {&newTexture.image.get(), 1});
		result == vk::Result::eSuccess) {
		newTexture.imageMemory = std::move(imageMemory);
	} else {
		Helpers::panic("Error allocating depth render-texture memory: %s\n", vk::to_string(result).c_str());
	}

	if (auto createResult = device->createImageViewUnique(viewInfo); createResult.result == vk::Result::eSuccess) {
		newTexture.imageView = std::move(createResult.value);
	} else {
		Helpers::panic("Error creating depth render-texture: %s\n", vk::to_string(createResult.result).c_str());
	}

	// Initial layout transition
	getCurrentCommandBuffer().pipelineBarrier(
		vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlags{}, {}, {},
		{vk::ImageMemoryBarrier(
			vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eShaderRead, vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, newTexture.image.get(), viewInfo.subresourceRange
		)}
	);

	return newTexture;
}

vk::RenderPass RendererVK::getRenderPass(vk::Format colorFormat, std::optional<vk::Format> depthFormat) {
	u64 renderPassHash = static_cast<u32>(colorFormat);

	if (depthFormat.has_value()) {
		renderPassHash |= (static_cast<u64>(depthFormat.value()) << 32);
	}

	// Cache hit
	if (renderPassCache.contains(renderPassHash)) {
		return renderPassCache.at(renderPassHash).get();
	}

	// Cache miss
	vk::RenderPassCreateInfo renderPassInfo = {};
	vk::SubpassDescription subPass = {};

	std::vector<vk::AttachmentDescription> renderPassAttachments = {};

	vk::AttachmentDescription colorAttachment = {};
	colorAttachment.format = colorFormat;
	colorAttachment.samples = vk::SampleCountFlagBits::e1;
	colorAttachment.loadOp = vk::AttachmentLoadOp::eLoad;
	colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
	colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eLoad;
	colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eStore;
	colorAttachment.initialLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	colorAttachment.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	renderPassAttachments.emplace_back(colorAttachment);

	if (depthFormat.has_value()) {
		vk::AttachmentDescription depthAttachment = {};
		depthAttachment.format = depthFormat.value();
		depthAttachment.samples = vk::SampleCountFlagBits::e1;
		depthAttachment.loadOp = vk::AttachmentLoadOp::eLoad;
		depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
		depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eLoad;
		depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eStore;
		depthAttachment.initialLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		depthAttachment.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		renderPassAttachments.emplace_back(depthAttachment);
	}

	renderPassInfo.setAttachments(renderPassAttachments);

	static const vk::AttachmentReference colorAttachmentReference = {0, vk::ImageLayout::eColorAttachmentOptimal};
	static const vk::AttachmentReference depthAttachmentReference = {1, vk::ImageLayout::eDepthStencilReadOnlyOptimal};

	subPass.setColorAttachments(colorAttachmentReference);
	if (depthFormat.has_value()) {
		subPass.setPDepthStencilAttachment(&depthAttachmentReference);
	}

	subPass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;

	renderPassInfo.setSubpasses(subPass);

	// We only have one sub-pass and we want all render-passes to be sequential,
	// so input/output depends on VK_SUBPASS_EXTERNAL
	static const vk::SubpassDependency subpassDependencies[2] = {
		vk::SubpassDependency(
			VK_SUBPASS_EXTERNAL, 0, vk::PipelineStageFlagBits::eAllGraphics, vk::PipelineStageFlagBits::eAllGraphics,
			vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eColorAttachmentWrite, vk::DependencyFlagBits::eByRegion
		),
		vk::SubpassDependency(
			0, VK_SUBPASS_EXTERNAL, vk::PipelineStageFlagBits::eAllGraphics, vk::PipelineStageFlagBits::eAllGraphics,
			vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eColorAttachmentWrite, vk::DependencyFlagBits::eByRegion
		)};

	renderPassInfo.setDependencies(subpassDependencies);

	if (auto createResult = device->createRenderPassUnique(renderPassInfo); createResult.result == vk::Result::eSuccess) {
		return (renderPassCache[renderPassHash] = std::move(createResult.value)).get();
	} else {
		Helpers::panic("Error creating render pass: %s\n", vk::to_string(createResult.result).c_str());
	}
	return {};
}

vk::RenderPass RendererVK::getRenderPass(PICA::ColorFmt colorFormat, std::optional<PICA::DepthFmt> depthFormat) {
	if (depthFormat.has_value()) {
		return getRenderPass(Vulkan::colorFormatToVulkan(colorFormat), Vulkan::depthFormatToVulkan(depthFormat.value()));
	} else {
		return getRenderPass(Vulkan::colorFormatToVulkan(colorFormat), {});
	}
}

vk::Result RendererVK::recreateSwapchain(vk::SurfaceKHR surface, vk::Extent2D swapchainExtent) {
	static constexpr u32 screenTextureWidth = 400;       // Top screen is 400 pixels wide, bottom is 320
	static constexpr u32 screenTextureHeight = 2 * 240;  // Both screens are 240 pixels tall
	static constexpr vk::ImageUsageFlags swapchainUsageFlagsRequired =
		(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst);

	// Extent + Image count + Usage + Surface Transform
	vk::ImageUsageFlags swapchainImageUsage;
	vk::SurfaceTransformFlagBitsKHR swapchainSurfaceTransform;
	if (const auto getResult = physicalDevice.getSurfaceCapabilitiesKHR(surface); getResult.result == vk::Result::eSuccess) {
		const vk::SurfaceCapabilitiesKHR& surfaceCapabilities = getResult.value;

		// In the case if width == height == -1, we define the extent ourselves but must fit within the limits
		if (surfaceCapabilities.currentExtent.width == -1 || surfaceCapabilities.currentExtent.height == -1) {
			swapchainExtent.width = std::max(swapchainExtent.width, surfaceCapabilities.minImageExtent.width);
			swapchainExtent.height = std::max(swapchainExtent.height, surfaceCapabilities.minImageExtent.height);
			swapchainExtent.width = std::min(swapchainExtent.width, surfaceCapabilities.maxImageExtent.width);
			swapchainExtent.height = std::min(swapchainExtent.height, surfaceCapabilities.maxImageExtent.height);
		}

		swapchainImageCount = surfaceCapabilities.minImageCount + 1;
		if ((surfaceCapabilities.maxImageCount > 0) && (swapchainImageCount > surfaceCapabilities.maxImageCount)) {
			swapchainImageCount = surfaceCapabilities.maxImageCount;
		}

		swapchainImageUsage = surfaceCapabilities.supportedUsageFlags & swapchainUsageFlagsRequired;

		if ((swapchainImageUsage & swapchainUsageFlagsRequired) != swapchainUsageFlagsRequired) {
			Helpers::panic(
				"Unsupported swapchain image usage. Could not acquire %s\n", vk::to_string(swapchainImageUsage ^ swapchainUsageFlagsRequired).c_str()
			);
		}

		if (surfaceCapabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity) {
			swapchainSurfaceTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
		} else {
			swapchainSurfaceTransform = surfaceCapabilities.currentTransform;
		}
	} else {
		Helpers::panic("Error getting surface capabilities: %s\n", vk::to_string(getResult.result).c_str());
	}

	// Preset Mode
	// Fifo support is required by all vulkan implementations, waits for vsync
	vk::PresentModeKHR swapchainPresentMode = vk::PresentModeKHR::eFifo;
	if (auto getResult = physicalDevice.getSurfacePresentModesKHR(surface); getResult.result == vk::Result::eSuccess) {
		const std::vector<vk::PresentModeKHR>& presentModes = getResult.value;

		// Use mailbox if available, lowest-latency vsync-enabled mode
		if (std::find(presentModes.begin(), presentModes.end(), vk::PresentModeKHR::eMailbox) != presentModes.end()) {
			swapchainPresentMode = vk::PresentModeKHR::eMailbox;
		}
	} else {
		Helpers::panic("Error enumerating surface present modes: %s\n", vk::to_string(getResult.result).c_str());
	}

	// Surface format
	vk::SurfaceFormatKHR swapchainSurfaceFormat;
	if (auto getResult = physicalDevice.getSurfaceFormatsKHR(surface); getResult.result == vk::Result::eSuccess) {
		std::vector<vk::SurfaceFormatKHR>& surfaceFormats = getResult.value;

		// A singular undefined surface format means we can use any format we want
		if ((surfaceFormats.size() == 1) && surfaceFormats[0].format == vk::Format::eUndefined) {
			// Assume R8G8B8A8-SRGB by default
			swapchainSurfaceFormat = {vk::Format::eR8G8B8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear};
		} else {
			// Find the next-best R8G8B8A8-SRGB format
			std::vector<vk::SurfaceFormatKHR>::iterator partitionEnd = surfaceFormats.end();

			const auto preferR8G8B8A8 = [](const vk::SurfaceFormatKHR& surfaceFormat) -> bool {
				return surfaceFormat.format == vk::Format::eR8G8B8A8Snorm;
			};
			partitionEnd = std::stable_partition(surfaceFormats.begin(), partitionEnd, preferR8G8B8A8);

			const auto preferSrgbNonLinear = [](const vk::SurfaceFormatKHR& surfaceFormat) -> bool {
				return surfaceFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
			};
			partitionEnd = std::stable_partition(surfaceFormats.begin(), partitionEnd, preferSrgbNonLinear);

			swapchainSurfaceFormat = surfaceFormats.front();
		}

	} else {
		Helpers::panic("Error enumerating surface formats: %s\n", vk::to_string(getResult.result).c_str());
	}

	vk::SwapchainCreateInfoKHR swapchainInfo = {};

	swapchainInfo.surface = surface;
	swapchainInfo.minImageCount = swapchainImageCount;
	swapchainInfo.imageFormat = swapchainSurfaceFormat.format;
	swapchainInfo.imageColorSpace = swapchainSurfaceFormat.colorSpace;
	swapchainInfo.imageExtent = swapchainExtent;
	swapchainInfo.imageArrayLayers = 1;
	swapchainInfo.imageUsage = swapchainImageUsage;
	swapchainInfo.imageSharingMode = vk::SharingMode::eExclusive;
	swapchainInfo.preTransform = swapchainSurfaceTransform;
	swapchainInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	swapchainInfo.presentMode = swapchainPresentMode;
	swapchainInfo.clipped = true;
	swapchainInfo.oldSwapchain = swapchain.get();

	if (auto createResult = device->createSwapchainKHRUnique(swapchainInfo); createResult.result == vk::Result::eSuccess) {
		swapchain = std::move(createResult.value);
	} else {
		Helpers::panic("Error creating swapchain: %s\n", vk::to_string(createResult.result).c_str());
	}

	// Get swapchain images
	if (auto getResult = device->getSwapchainImagesKHR(swapchain.get()); getResult.result == vk::Result::eSuccess) {
		swapchainImages = getResult.value;
		swapchainImageViews.resize(swapchainImages.size());

		// Create image-views
		for (usize i = 0; i < swapchainImages.size(); i++) {
			vk::ImageViewCreateInfo viewInfo = {};
			viewInfo.image = swapchainImages[i];
			viewInfo.viewType = vk::ImageViewType::e2D;
			viewInfo.format = swapchainSurfaceFormat.format;
			viewInfo.components = vk::ComponentMapping();
			viewInfo.subresourceRange = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

			if (auto createResult = device->createImageViewUnique(viewInfo); createResult.result == vk::Result::eSuccess) {
				swapchainImageViews[i] = std::move(createResult.value);
			} else {
				Helpers::panic("Error creating swapchain image-view: #%zu %s\n", i, vk::to_string(getResult.result).c_str());
			}
		}
	} else {
		Helpers::panic("Error creating acquiring swapchain images: %s\n", vk::to_string(getResult.result).c_str());
	}

	return vk::Result::eSuccess;
}

RendererVK::RendererVK(GPU& gpu, const std::array<u32, regNum>& internalRegs, const std::array<u32, extRegNum>& externalRegs)
	: Renderer(gpu, internalRegs, externalRegs) {}

RendererVK::~RendererVK() {}

void RendererVK::reset() { renderPassCache.clear(); }

void RendererVK::display() {
	// Get the next available swapchain image, and signal the semaphore when it's ready
	static constexpr u32 swapchainImageInvalid = std::numeric_limits<u32>::max();
	u32 swapchainImageIndex = swapchainImageInvalid;
	if (swapchain) {
		if (const auto acquireResult =
				device->acquireNextImageKHR(swapchain.get(), std::numeric_limits<u64>::max(), swapImageFreeSemaphore[frameBufferingIndex].get(), {});
			acquireResult.result == vk::Result::eSuccess) {
			swapchainImageIndex = acquireResult.value;
		} else {
			switch (acquireResult.result) {
				case vk::Result::eSuboptimalKHR:
				case vk::Result::eErrorOutOfDateKHR: {
					// Surface resized
					vk::Extent2D swapchainExtent;
					{
						int windowWidth, windowHeight;
						// Block until we have a valid surface-area to present to
						// Usually this is because the window has been minimized
						// Todo: We should still be rendering even without a valid swapchain
						do {
							SDL_Vulkan_GetDrawableSize(targetWindow, &windowWidth, &windowHeight);
						} while (!windowWidth || !windowHeight);
						swapchainExtent.width = windowWidth;
						swapchainExtent.height = windowHeight;
					}
					recreateSwapchain(swapchainSurface, swapchainExtent);
					break;
				}
				default: {
					Helpers::panic("Error acquiring next swapchain image: %s\n", vk::to_string(acquireResult.result).c_str());
				}
			}
		}
	}

	const bool topActiveFb = externalRegs[PICA::ExternalRegs::Framebuffer0Select] & 1;
	const u32 topScreenAddr = externalRegs[topActiveFb ? PICA::ExternalRegs::Framebuffer0AFirstAddr : PICA::ExternalRegs::Framebuffer0ASecondAddr];

	const bool bottomActiveFb = externalRegs[PICA::ExternalRegs::Framebuffer1Select] & 1;
	const u32 bottomScreenAddr =
		externalRegs[bottomActiveFb ? PICA::ExternalRegs::Framebuffer1AFirstAddr : PICA::ExternalRegs::Framebuffer1ASecondAddr];

	//// Render Display
	{
		static const std::array<float, 4> renderScreenScopeColor = {{1.0f, 0.0f, 1.0f, 1.0f}};

		Vulkan::DebugLabelScope debugScope(getCurrentCommandBuffer(), renderScreenScopeColor, "Render Screen");

		vk::RenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.renderPass = getRenderPass(vk::Format::eR8G8B8A8Unorm, {});

		renderPassBeginInfo.framebuffer = screenTextureFramebuffers[frameBufferingIndex].get();
		renderPassBeginInfo.renderArea.offset = vk::Offset2D();
		renderPassBeginInfo.renderArea.extent = vk::Extent2D(400, 240 * 2);

		getCurrentCommandBuffer().beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

		// Render top screen
		if (const Texture* topScreen = findRenderTexture(topScreenAddr); topScreen) {
			static const std::array<float, 4> scopeColor = {{1.0f, 0.0f, 0.0f, 1.0f}};
			Vulkan::DebugLabelScope debugScope(getCurrentCommandBuffer(), scopeColor, "Top Screen: %08x", topScreenAddr);

			descriptorUpdateBatch->addImageSampler(
				topDisplayPipelineDescriptorSet[frameBufferingIndex], 0, topScreen->imageView.get(), samplerCache->getSampler(sampler2D())
			);
			getCurrentCommandBuffer().bindDescriptorSets(
				vk::PipelineBindPoint::eGraphics, displayPipelineLayout.get(), 0, {topDisplayPipelineDescriptorSet[frameBufferingIndex]}, {}
			);
			getCurrentCommandBuffer().bindPipeline(vk::PipelineBindPoint::eGraphics, displayPipeline.get());
			getCurrentCommandBuffer().setViewport(0, vk::Viewport(0, 0, 400, 240));
			getCurrentCommandBuffer().setScissor(0, vk::Rect2D({0, 0}, {400, 240}));
			getCurrentCommandBuffer().draw(3, 1, 0, 0);
		}

		// Render bottom screen
		if (const Texture* bottomScreen = findRenderTexture(bottomScreenAddr); bottomScreen) {
			static const std::array<float, 4> scopeColor = {{0.0f, 1.0f, 0.0f, 1.0f}};
			Vulkan::DebugLabelScope debugScope(getCurrentCommandBuffer(), scopeColor, "Bottom Screen: %08x", bottomScreenAddr);

			descriptorUpdateBatch->addImageSampler(
				bottomDisplayPipelineDescriptorSet[frameBufferingIndex], 0, bottomScreen->imageView.get(), samplerCache->getSampler(sampler2D())
			);
			getCurrentCommandBuffer().bindDescriptorSets(
				vk::PipelineBindPoint::eGraphics, displayPipelineLayout.get(), 0, {bottomDisplayPipelineDescriptorSet[frameBufferingIndex]}, {}
			);
			getCurrentCommandBuffer().bindPipeline(vk::PipelineBindPoint::eGraphics, displayPipeline.get());
			getCurrentCommandBuffer().setViewport(0, vk::Viewport(40, 240, 320, 240));
			getCurrentCommandBuffer().setScissor(0, vk::Rect2D({40, 240}, {320, 240}));
			getCurrentCommandBuffer().draw(3, 1, 0, 0);
		}

		getCurrentCommandBuffer().endRenderPass();
	}

	//// Present
	if (swapchainImageIndex != swapchainImageInvalid) {
		static const std::array<float, 4> presentScopeColor = {{1.0f, 1.0f, 1.0f, 1.0f}};
		Vulkan::DebugLabelScope debugScope(getCurrentCommandBuffer(), presentScopeColor, "Present");

		// Prepare swapchain image for color-clear/blit-dst, prepare top/bottom screen for blit-src
		getCurrentCommandBuffer().pipelineBarrier(
			vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {},
			{
				// swapchainImage: Undefined -> TransferDst
				vk::ImageMemoryBarrier(
					vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eTransferWrite, vk::ImageLayout::eUndefined,
					vk::ImageLayout::eTransferDstOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, swapchainImages[swapchainImageIndex],
					vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
				),
				// screenTexture: ShaderReadOnlyOptimal -> TransferSrc
				vk::ImageMemoryBarrier(
					vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eTransferRead, vk::ImageLayout::eShaderReadOnlyOptimal,
					vk::ImageLayout::eTransferSrcOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, screenTexture[frameBufferingIndex].get(),
					vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
				),
			}
		);

		// Clear swapchain image with black
		static const std::array<float, 4> clearColor = {{0.0f, 0.0f, 0.0f, 1.0f}};
		getCurrentCommandBuffer().clearColorImage(
			swapchainImages[swapchainImageIndex], vk::ImageLayout::eTransferDstOptimal, clearColor,
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
		);

		// Blit screentexture into swapchain image
		static const vk::ImageBlit screenBlit(
			vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), {vk::Offset3D{}, vk::Offset3D{400, 240 * 2, 1}},
			vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), {vk::Offset3D{}, vk::Offset3D{400, 240 * 2, 1}}
		);
		getCurrentCommandBuffer().blitImage(
			screenTexture[frameBufferingIndex].get(), vk::ImageLayout::eTransferSrcOptimal, swapchainImages[swapchainImageIndex],
			vk::ImageLayout::eTransferDstOptimal, {screenBlit}, vk::Filter::eNearest
		);

		// Prepare swapchain image for present
		// Transfer screenTexture back into ColorAttachmentOptimal
		getCurrentCommandBuffer().pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllGraphics, vk::DependencyFlags(), {}, {},
			{
				// swapchainImage: TransferDst -> Preset (wait for all writes)
				vk::ImageMemoryBarrier(
					vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eColorAttachmentWrite, vk::ImageLayout::eTransferDstOptimal,
					vk::ImageLayout::ePresentSrcKHR, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, swapchainImages[swapchainImageIndex],
					vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
				),
				// screenTexture: TransferSrc -> eShaderReadOnlyOptimal (wait for all reads)
				vk::ImageMemoryBarrier(
					vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eShaderRead, vk::ImageLayout::eTransferSrcOptimal,
					vk::ImageLayout::eShaderReadOnlyOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
					screenTexture[frameBufferingIndex].get(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
				),
			}
		);
	}

	if (const vk::Result endResult = getCurrentCommandBuffer().end(); endResult != vk::Result::eSuccess) {
		Helpers::panic("Error ending command buffer recording: %s\n", vk::to_string(endResult).c_str());
	}

	// Flush all descriptor writes
	descriptorUpdateBatch->flush();

	vk::SubmitInfo submitInfo = {};
	// Wait for any previous uses of the image image to finish presenting
	std::vector<vk::Semaphore> waitSemaphores;
	std::vector<vk::PipelineStageFlags> waitSemaphoreStages;
	{
		if (swapchainImageIndex != swapchainImageInvalid) {
			waitSemaphores.emplace_back(swapImageFreeSemaphore[frameBufferingIndex].get());
			static const vk::PipelineStageFlags waitStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
			waitSemaphoreStages.emplace_back(waitStageMask);
		}

		// Ensure a proper semaphore wait on render-finished
		// We already wait on the fence, but this must be done to be compliant
		// to validation layers
		waitSemaphores.emplace_back(renderFinishedSemaphore[frameBufferingIndex].get());
		waitSemaphoreStages.emplace_back(vk::PipelineStageFlagBits::eColorAttachmentOutput);

		submitInfo.setWaitSemaphores(waitSemaphores);
		submitInfo.setWaitDstStageMask(waitSemaphoreStages);
	}
	// Signal when finished
	submitInfo.setSignalSemaphores(renderFinishedSemaphore[frameBufferingIndex].get());

	submitInfo.setCommandBuffers(getCurrentCommandBuffer());

	device->resetFences({frameFinishedFences[frameBufferingIndex].get()});

	if (const vk::Result submitResult = graphicsQueue.submit({submitInfo}, frameFinishedFences[frameBufferingIndex].get());
		submitResult != vk::Result::eSuccess) {
		Helpers::panic("Error submitting to graphics queue: %s\n", vk::to_string(submitResult).c_str());
	}

	if (swapchainImageIndex != swapchainImageInvalid) {
		vk::PresentInfoKHR presentInfo = {};
		presentInfo.setWaitSemaphores(renderFinishedSemaphore[frameBufferingIndex].get());
		presentInfo.setSwapchains(swapchain.get());
		presentInfo.setImageIndices(swapchainImageIndex);

		if (const auto presentResult = presentQueue.presentKHR(presentInfo); presentResult == vk::Result::eSuccess) {
		} else {
			switch (presentResult) {
				case vk::Result::eSuboptimalKHR:
				case vk::Result::eErrorOutOfDateKHR: {
					// Surface resized
					vk::Extent2D swapchainExtent;
					{
						int windowWidth, windowHeight;
						SDL_Vulkan_GetDrawableSize(targetWindow, &windowWidth, &windowHeight);
						swapchainExtent.width = windowWidth;
						swapchainExtent.height = windowHeight;
					}
					recreateSwapchain(swapchainSurface, swapchainExtent);
					break;
				}
				default: {
					Helpers::panic("Error presenting swapchain image: %s\n", vk::to_string(presentResult).c_str());
				}
			}
		}
	}

	// We are now working on the next frame
	frameBufferingIndex = ((frameBufferingIndex + 1) % frameBufferingCount);

	// Wait for next frame to be ready

	// Block, on the CPU, to ensure that this buffered-frame is ready for more work
	if (auto waitResult = device->waitForFences({frameFinishedFences[frameBufferingIndex].get()}, true, std::numeric_limits<u64>::max());
		waitResult != vk::Result::eSuccess) {
		Helpers::panic("Error waiting on swapchain fence: %s\n", vk::to_string(waitResult).c_str());
	}

	{
		frameFramebuffers[frameBufferingIndex].clear();

		getCurrentCommandBuffer().reset();

		vk::CommandBufferBeginInfo beginInfo = {};
		beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;

		if (const vk::Result beginResult = getCurrentCommandBuffer().begin(beginInfo); beginResult != vk::Result::eSuccess) {
			Helpers::panic("Error beginning command buffer recording: %s\n", vk::to_string(beginResult).c_str());
		}
	}
}

void RendererVK::initGraphicsContext(SDL_Window* window) {
	targetWindow = window;
	// Resolve all instance function pointers
	static vk::DynamicLoader dl;
	VULKAN_HPP_DEFAULT_DISPATCHER.init(dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"));

	// Create Instance
	vk::ApplicationInfo applicationInfo = {};
	applicationInfo.apiVersion = VK_API_VERSION_1_1;

	applicationInfo.pEngineName = "Alber";
	applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);

	applicationInfo.pApplicationName = "Alber";
	applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);

	vk::InstanceCreateInfo instanceInfo = {};

	instanceInfo.pApplicationInfo = &applicationInfo;

	std::unordered_set<std::string> instanceExtensionsAvailable = {};
	if (const auto enumerateResult = vk::enumerateInstanceExtensionProperties(); enumerateResult.result == vk::Result::eSuccess) {
		for (const auto& curExtension : enumerateResult.value) {
			instanceExtensionsAvailable.emplace(curExtension.extensionName.data());
		}
	}

	std::vector<const char*> instanceExtensions = {};

	if (instanceExtensionsAvailable.contains(VK_KHR_SURFACE_EXTENSION_NAME)) {
		instanceExtensions.emplace_back(VK_KHR_SURFACE_EXTENSION_NAME);
	}

	bool debugUtils = false;
	if (instanceExtensionsAvailable.contains(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
		instanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		debugUtils = true;
	}

#if defined(__APPLE__)
	if (instanceExtensionsAvailable.contains(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
		instanceExtensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
	}
#endif

	// Get any additional extensions that SDL wants as well
	if (targetWindow) {
		unsigned int extensionCount = 0;
		SDL_Vulkan_GetInstanceExtensions(targetWindow, &extensionCount, nullptr);
		std::vector<const char*> sdlInstanceExtensions(extensionCount);
		SDL_Vulkan_GetInstanceExtensions(targetWindow, &extensionCount, sdlInstanceExtensions.data());

		instanceExtensions.insert(instanceExtensions.end(), sdlInstanceExtensions.begin(), sdlInstanceExtensions.end());
	}

#if defined(__APPLE__)
	instanceInfo.flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
#endif

	instanceInfo.ppEnabledExtensionNames = instanceExtensions.data();
	instanceInfo.enabledExtensionCount = instanceExtensions.size();

	if (auto createResult = vk::createInstanceUnique(instanceInfo); createResult.result == vk::Result::eSuccess) {
		instance = std::move(createResult.value);
	} else {
		Helpers::panic("Error creating Vulkan instance: %s\n", vk::to_string(createResult.result).c_str());
	}
	// Initialize instance-specific function pointers
	VULKAN_HPP_DEFAULT_DISPATCHER.init(instance.get());

	// Enable debug messenger if the instance was able to be created with debug_utils
	if (debugUtils) {
		vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		debugCreateInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
										  vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning;
		debugCreateInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
									  vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral;
		debugCreateInfo.pfnUserCallback = &Vulkan::debugMessageCallback;
		if (auto createResult = instance->createDebugUtilsMessengerEXTUnique(debugCreateInfo); createResult.result == vk::Result::eSuccess) {
			debugMessenger = std::move(createResult.value);
		} else {
			Helpers::warn("Error registering debug messenger: %s", vk::to_string(createResult.result).c_str());
		}
	}

	// Create surface
	if (window) {
		if (VkSurfaceKHR newSurface; SDL_Vulkan_CreateSurface(window, instance.get(), &newSurface)) {
			swapchainSurface = newSurface;
		} else {
			Helpers::warn("Error creating Vulkan surface");
		}
	}

	// Pick physical device
	if (auto enumerateResult = instance->enumeratePhysicalDevices(); enumerateResult.result == vk::Result::eSuccess) {
		std::vector<vk::PhysicalDevice> physicalDevices = std::move(enumerateResult.value);
		std::vector<vk::PhysicalDevice>::iterator partitionEnd = physicalDevices.end();

		// Prefer GPUs that can access the surface
		if (swapchainSurface) {
			const auto surfaceSupport = [this](const vk::PhysicalDevice& physicalDevice) -> bool {
				const usize queueCount = physicalDevice.getQueueFamilyProperties().size();
				for (usize queueIndex = 0; queueIndex < queueCount; ++queueIndex) {
					if (auto supportResult = physicalDevice.getSurfaceSupportKHR(queueIndex, swapchainSurface);
						supportResult.result == vk::Result::eSuccess) {
						return supportResult.value;
					}
				}
				return false;
			};

			partitionEnd = std::stable_partition(physicalDevices.begin(), partitionEnd, surfaceSupport);
		}

		// Prefer Discrete GPUs
		const auto isDiscrete = [](const vk::PhysicalDevice& physicalDevice) -> bool {
			return physicalDevice.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
		};
		partitionEnd = std::stable_partition(physicalDevices.begin(), partitionEnd, isDiscrete);

		// Pick the "best" out of all of the previous criteria, preserving the order that the
		// driver gave us the devices in(ex: optimus configuration)
		physicalDevice = physicalDevices.front();
	} else {
		Helpers::panic("Error enumerating physical devices: %s\n", vk::to_string(enumerateResult.result).c_str());
	}

	// Get device queues

	std::vector<vk::DeviceQueueCreateInfo> deviceQueueInfos;
	{
		const std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
		std::unordered_set<u32> queueFamilyRequests;
		// Get present queue family
		if (swapchainSurface) {
			for (usize queueFamilyIndex = 0; queueFamilyIndex < queueFamilyProperties.size(); ++queueFamilyIndex) {
				if (auto supportResult = physicalDevice.getSurfaceSupportKHR(queueFamilyIndex, swapchainSurface);
					supportResult.result == vk::Result::eSuccess) {
					if (supportResult.value) {
						presentQueueFamily = queueFamilyIndex;
						break;
					}
				}
			}
			queueFamilyRequests.emplace(presentQueueFamily);
		}

		static const float queuePriority = 1.0f;

		graphicsQueueFamily = findQueueFamily(queueFamilyProperties, vk::QueueFlagBits::eGraphics);
		queueFamilyRequests.emplace(graphicsQueueFamily);
		computeQueueFamily = findQueueFamily(queueFamilyProperties, vk::QueueFlagBits::eCompute);
		queueFamilyRequests.emplace(computeQueueFamily);
		transferQueueFamily = findQueueFamily(queueFamilyProperties, vk::QueueFlagBits::eTransfer);
		queueFamilyRequests.emplace(transferQueueFamily);

		// Requests a singular queue for each unique queue-family

		for (const u32 queueFamilyIndex : queueFamilyRequests) {
			deviceQueueInfos.emplace_back(vk::DeviceQueueCreateInfo({}, queueFamilyIndex, 1, &queuePriority));
		}
	}

	// Create Device
	vk::DeviceCreateInfo deviceInfo = {};

	// Device extensions
	std::vector<const char*> deviceExtensions = {
#if defined(__APPLE__)
		"VK_KHR_portability_subset",
#endif
		// VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME
	};

	std::unordered_set<std::string> physicalDeviceExtensions;
	if (const auto enumerateResult = physicalDevice.enumerateDeviceExtensionProperties(); enumerateResult.result == vk::Result::eSuccess) {
		for (const auto& extension : enumerateResult.value) {
			physicalDeviceExtensions.insert(extension.extensionName);
		}
	} else {
		Helpers::panic("Error enumerating physical devices extensions: %s\n", vk::to_string(enumerateResult.result).c_str());
	}

	// Opertional extensions

	// Optionally enable the swapchain, to support "headless" rendering
	if (physicalDeviceExtensions.contains(VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
		deviceExtensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	}

	deviceInfo.setPEnabledExtensionNames(deviceExtensions);

	vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceTimelineSemaphoreFeatures> deviceFeatureChain = {};

	auto& deviceFeatures = deviceFeatureChain.get<vk::PhysicalDeviceFeatures2>().features;

	auto& deviceTimelineFeatures = deviceFeatureChain.get<vk::PhysicalDeviceTimelineSemaphoreFeatures>();
	// deviceTimelineFeatures.timelineSemaphore = true;

	deviceInfo.pNext = &deviceFeatureChain.get();

	deviceInfo.setQueueCreateInfos(deviceQueueInfos);

	if (auto createResult = physicalDevice.createDeviceUnique(deviceInfo); createResult.result == vk::Result::eSuccess) {
		device = std::move(createResult.value);
	} else {
		Helpers::panic("Error creating logical device: %s\n", vk::to_string(createResult.result).c_str());
	}

	// Initialize device-specific function pointers
	VULKAN_HPP_DEFAULT_DISPATCHER.init(device.get());

	if (presentQueueFamily != VK_QUEUE_FAMILY_IGNORED) {
		presentQueue = device->getQueue(presentQueueFamily, 0);
	}
	graphicsQueue = device->getQueue(graphicsQueueFamily, 0);
	computeQueue = device->getQueue(computeQueueFamily, 0);
	transferQueue = device->getQueue(transferQueueFamily, 0);

	// Command pool
	vk::CommandPoolCreateInfo commandPoolInfo = {};
	commandPoolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

	if (auto createResult = device->createCommandPoolUnique(commandPoolInfo); createResult.result == vk::Result::eSuccess) {
		commandPool = std::move(createResult.value);
	} else {
		Helpers::panic("Error creating command pool: %s\n", vk::to_string(createResult.result).c_str());
	}

	// Create swapchain
	if (targetWindow && swapchainSurface) {
		vk::Extent2D swapchainExtent;
		{
			int windowWidth, windowHeight;
			SDL_Vulkan_GetDrawableSize(window, &windowWidth, &windowHeight);
			swapchainExtent.width = windowWidth;
			swapchainExtent.height = windowHeight;
		}
		recreateSwapchain(swapchainSurface, swapchainExtent);
	}

	// Create frame-buffering data
	// Frame-buffering Command buffer(s)
	vk::CommandBufferAllocateInfo commandBuffersInfo = {};
	commandBuffersInfo.commandPool = commandPool.get();
	commandBuffersInfo.level = vk::CommandBufferLevel::ePrimary;
	commandBuffersInfo.commandBufferCount = frameBufferingCount;

	if (auto allocateResult = device->allocateCommandBuffersUnique(commandBuffersInfo); allocateResult.result == vk::Result::eSuccess) {
		frameCommandBuffers = std::move(allocateResult.value);
	} else {
		Helpers::panic("Error allocating command buffer: %s\n", vk::to_string(allocateResult.result).c_str());
	}

	// Initialize the first command buffer to be in the RECORDING state
	vk::CommandBufferBeginInfo beginInfo = {};
	beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;

	if (const vk::Result beginResult = frameCommandBuffers[frameBufferingIndex]->begin(beginInfo); beginResult != vk::Result::eSuccess) {
		Helpers::panic("Error beginning command buffer recording: %s\n", vk::to_string(beginResult).c_str());
	}

	// Frame-buffering synchronization primitives
	vk::FenceCreateInfo fenceInfo = {};
	fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

	vk::SemaphoreCreateInfo semaphoreInfo = {};

	swapImageFreeSemaphore.resize(frameBufferingCount);
	renderFinishedSemaphore.resize(frameBufferingCount);
	frameFinishedFences.resize(frameBufferingCount);
	frameFramebuffers.resize(frameBufferingCount);
	frameCommandBuffers.resize(frameBufferingCount);

	vk::ImageCreateInfo screenTextureInfo = {};
	screenTextureInfo.setImageType(vk::ImageType::e2D);
	screenTextureInfo.setFormat(vk::Format::eR8G8B8A8Unorm);
	screenTextureInfo.setExtent(vk::Extent3D(400, 240 * 2, 1));
	screenTextureInfo.setMipLevels(1);
	screenTextureInfo.setArrayLayers(1);
	screenTextureInfo.setSamples(vk::SampleCountFlagBits::e1);
	screenTextureInfo.setTiling(vk::ImageTiling::eOptimal);
	screenTextureInfo.setUsage(
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment | vk::ImageUsageFlagBits::eTransferSrc |
		vk::ImageUsageFlagBits::eTransferDst
	);
	screenTextureInfo.setSharingMode(vk::SharingMode::eExclusive);
	screenTextureInfo.setInitialLayout(vk::ImageLayout::eUndefined);

	screenTexture.resize(frameBufferingCount);
	screenTextureViews.resize(frameBufferingCount);
	screenTextureFramebuffers.resize(frameBufferingCount);

	for (usize i = 0; i < frameBufferingCount; ++i) {
		if (auto createResult = device->createSemaphoreUnique(semaphoreInfo); createResult.result == vk::Result::eSuccess) {
			swapImageFreeSemaphore[i] = std::move(createResult.value);

			Vulkan::setObjectName(device.get(), swapImageFreeSemaphore[i].get(), "swapImageFreeSemaphore#%zu", i);
		} else {
			Helpers::panic("Error creating 'present-ready' semaphore: %s\n", vk::to_string(createResult.result).c_str());
		}

		if (auto createResult = device->createSemaphoreUnique(semaphoreInfo); createResult.result == vk::Result::eSuccess) {
			renderFinishedSemaphore[i] = std::move(createResult.value);

			Vulkan::setObjectName(device.get(), renderFinishedSemaphore[i].get(), "renderFinishedSemaphore#%zu", i);
		} else {
			Helpers::panic("Error creating 'post-render' semaphore: %s\n", vk::to_string(createResult.result).c_str());
		}

		if (auto createResult = device->createFenceUnique(fenceInfo); createResult.result == vk::Result::eSuccess) {
			frameFinishedFences[i] = std::move(createResult.value);
		} else {
			Helpers::panic("Error creating 'frame-finished' fence: %s\n", vk::to_string(createResult.result).c_str());
		}

		if (auto createResult = device->createImageUnique(screenTextureInfo); createResult.result == vk::Result::eSuccess) {
			screenTexture[i] = std::move(createResult.value);

			Vulkan::setObjectName(device.get(), screenTexture[i].get(), "screenTexture#%zu", i);
		} else {
			Helpers::panic("Error creating top-screen image: %s\n", vk::to_string(createResult.result).c_str());
		}
	}

	// Commit memory to all of our images
	{
		const auto getImage = [](const vk::UniqueImage& image) -> vk::Image { return image.get(); };
		std::vector<vk::Image> images;
		std::transform(screenTexture.begin(), screenTexture.end(), std::back_inserter(images), getImage);

		if (auto [result, imageMemory] = Vulkan::commitImageHeap(device.get(), physicalDevice, images); result == vk::Result::eSuccess) {
			framebufferMemory = std::move(imageMemory);
		} else {
			Helpers::panic("Error allocating framebuffer memory: %s\n", vk::to_string(result).c_str());
		}
	}

	// Memory is bounded, create views, framebuffer, and layout transitions for screentexture
	vk::ImageViewCreateInfo screenTextureViewCreateInfo = {};
	screenTextureViewCreateInfo.viewType = vk::ImageViewType::e2D;
	screenTextureViewCreateInfo.format = vk::Format::eR8G8B8A8Unorm;
	screenTextureViewCreateInfo.components.r = vk::ComponentSwizzle::eR;
	screenTextureViewCreateInfo.components.g = vk::ComponentSwizzle::eG;
	screenTextureViewCreateInfo.components.b = vk::ComponentSwizzle::eB;
	screenTextureViewCreateInfo.components.a = vk::ComponentSwizzle::eA;
	screenTextureViewCreateInfo.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

	for (usize i = 0; i < frameBufferingCount; ++i) {
		screenTextureViewCreateInfo.image = screenTexture[i].get();

		if (auto createResult = device->createImageViewUnique(screenTextureViewCreateInfo); createResult.result == vk::Result::eSuccess) {
			screenTextureViews[i] = std::move(createResult.value);
		} else {
			Helpers::panic("Error creating screen texture view: %s\n", vk::to_string(createResult.result).c_str());
		}

		// Initial layout transition
		getCurrentCommandBuffer().pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlags{}, {}, {},
			{vk::ImageMemoryBarrier(
				vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eShaderRead, vk::ImageLayout::eUndefined,
				vk::ImageLayout::eShaderReadOnlyOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, screenTexture[i].get(),
				screenTextureViewCreateInfo.subresourceRange
			)}
		);

		vk::FramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.setRenderPass(getRenderPass(vk::Format::eR8G8B8A8Unorm, {}));
		framebufferInfo.setAttachments(screenTextureViews[i].get());
		framebufferInfo.setWidth(400);
		framebufferInfo.setHeight(240 * 2);
		framebufferInfo.setLayers(1);
		if (auto createResult = device->createFramebufferUnique(framebufferInfo); createResult.result == vk::Result::eSuccess) {
			screenTextureFramebuffers[i] = std::move(createResult.value);
		} else {
			Helpers::panic("Error creating screen-texture framebuffer: %s\n", vk::to_string(createResult.result).c_str());
		}
	}

	static vk::DescriptorSetLayoutBinding displayShaderLayout[] = {
		{// Just a singular texture slot
		 0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},
	};

	if (auto createResult = Vulkan::DescriptorUpdateBatch::create(device.get()); createResult.has_value()) {
		descriptorUpdateBatch = std::make_unique<Vulkan::DescriptorUpdateBatch>(std::move(createResult.value()));
	} else {
		Helpers::panic("Error creating descriptor update batch\n");
	}

	if (auto createResult = Vulkan::SamplerCache::create(device.get()); createResult.has_value()) {
		samplerCache = std::make_unique<Vulkan::SamplerCache>(std::move(createResult.value()));
	} else {
		Helpers::panic("Error creating sampler cache\n");
	}

	if (auto createResult = Vulkan::DescriptorHeap::create(device.get(), displayShaderLayout); createResult.has_value()) {
		displayDescriptorHeap = std::make_unique<Vulkan::DescriptorHeap>(std::move(createResult.value()));
	} else {
		Helpers::panic("Error creating descriptor heap\n");
	}

	for (usize i = 0; i < frameBufferingCount; ++i) {
		if (auto allocateResult = displayDescriptorHeap->allocateDescriptorSet(); allocateResult.has_value()) {
			topDisplayPipelineDescriptorSet.emplace_back(allocateResult.value());
		} else {
			Helpers::panic("Error creating descriptor set\n");
		}
		if (auto allocateResult = displayDescriptorHeap->allocateDescriptorSet(); allocateResult.has_value()) {
			bottomDisplayPipelineDescriptorSet.emplace_back(allocateResult.value());
		} else {
			Helpers::panic("Error creating descriptor set\n");
		}
	}

	auto vk_resources = cmrc::RendererVK::get_filesystem();
	auto displayVertexShader = vk_resources.open("vulkan_display.vert.spv");
	auto displayFragmentShader = vk_resources.open("vulkan_display.frag.spv");

	vk::UniqueShaderModule displayVertexShaderModule = createShaderModule(device.get(), displayVertexShader);
	vk::UniqueShaderModule displayFragmentShaderModule = createShaderModule(device.get(), displayFragmentShader);

	vk::RenderPass screenTextureRenderPass = getRenderPass(screenTextureInfo.format, {});

	std::tie(displayPipeline, displayPipelineLayout) = createGraphicsPipeline(
		device.get(), {}, {{displayDescriptorHeap.get()->getDescriptorSetLayout()}}, displayVertexShaderModule.get(),
		displayFragmentShaderModule.get(), {}, {}, screenTextureRenderPass
	);
}

void RendererVK::clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) {
	const Texture* renderTexture = findRenderTexture(startAddress);

	if (!renderTexture) {
		// not found
		return;
	}

	// Color-Clear
	{
		vk::ClearColorValue clearColor = {};

		clearColor.float32[0] = Helpers::getBits<24, 8>(value) / 255.0f;  // r
		clearColor.float32[1] = Helpers::getBits<16, 8>(value) / 255.0f;  // g
		clearColor.float32[2] = Helpers::getBits<8, 8>(value) / 255.0f;   // b
		clearColor.float32[3] = Helpers::getBits<0, 8>(value) / 255.0f;   // a

		Vulkan::DebugLabelScope scope(
			getCurrentCommandBuffer(), clearColor.float32, "ClearBuffer start:%08X end:%08X value:%08X control:%08X\n", startAddress, endAddress,
			value, control
		);

		getCurrentCommandBuffer().pipelineBarrier(
			vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {},
			{
				// renderTexture: ShaderReadOnlyOptimal -> TransferDst
				vk::ImageMemoryBarrier(
					vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite, vk::ImageLayout::eShaderReadOnlyOptimal,
					vk::ImageLayout::eTransferDstOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, renderTexture->image.get(),
					vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
				),
			}
		);

		// Clear RenderTarget
		getCurrentCommandBuffer().clearColorImage(
			renderTexture->image.get(), vk::ImageLayout::eTransferDstOptimal, clearColor,
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
		);

		getCurrentCommandBuffer().pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllGraphics, vk::DependencyFlags(), {}, {},
			{
				// renderTexture: TransferDst -> eShaderReadOnlyOptimal
				vk::ImageMemoryBarrier(
					vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead, vk::ImageLayout::eTransferDstOptimal,
					vk::ImageLayout::eShaderReadOnlyOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, renderTexture->image.get(),
					vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
				),
			}
		);
	}
}

// NOTE: The GPU format has RGB5551 and RGB655 swapped compared to internal regs format
static PICA::ColorFmt ToColorFmt(u32 format) {
	switch (format) {
		case 2: return PICA::ColorFmt::RGB565;
		case 3: return PICA::ColorFmt::RGBA5551;
		default: return static_cast<PICA::ColorFmt>(format);
	}
}

void RendererVK::displayTransfer(u32 inputAddr, u32 outputAddr, u32 inputSize, u32 outputSize, u32 flags) {
	const u32 inputWidth = inputSize & 0xffff;
	const u32 inputHeight = inputSize >> 16;
	const PICA::ColorFmt inputFormat = ToColorFmt(Helpers::getBits<8, 3>(flags));
	const PICA::ColorFmt outputFormat = ToColorFmt(Helpers::getBits<12, 3>(flags));
	const bool verticalFlip = flags & 1;
	const PICA::Scaling scaling = static_cast<PICA::Scaling>(Helpers::getBits<24, 2>(flags));

	u32 outputWidth = outputSize & 0xffff;
	u32 outputHeight = outputSize >> 16;

	Texture& srcFramebuffer = getColorRenderTexture(inputAddr, inputFormat, inputWidth, inputHeight);
	Math::Rect<u32> srcRect = srcFramebuffer.getSubRect(inputAddr, outputWidth, outputHeight);

	if (verticalFlip) {
		std::swap(srcRect.bottom, srcRect.top);
	}

	// Apply scaling for the destination rectangle.
	if (scaling == PICA::Scaling::X || scaling == PICA::Scaling::XY) {
		outputWidth >>= 1;
	}

	if (scaling == PICA::Scaling::XY) {
		outputHeight >>= 1;
	}

	Texture& destFramebuffer = getColorRenderTexture(outputAddr, outputFormat, outputWidth, outputHeight);
	Math::Rect<u32> destRect = destFramebuffer.getSubRect(outputAddr, outputWidth, outputHeight);

	if (inputWidth != outputWidth) {
		// Helpers::warn("Strided display transfer is not handled correctly!\n");
	}

	const vk::ImageBlit blitRegion(
		vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
		{vk::Offset3D{(int)srcRect.left, (int)srcRect.top, 0}, vk::Offset3D{(int)srcRect.right, (int)srcRect.bottom, 1}},
		vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
		{vk::Offset3D{(int)destRect.left, (int)destRect.top, 0}, vk::Offset3D{(int)destRect.right, (int)destRect.bottom, 1}}
	);

	const vk::CommandBuffer& blitCommandBuffer = getCurrentCommandBuffer();

	static const std::array<float, 4> displayTransferColor = {{1.0f, 1.0f, 0.0f, 1.0f}};
	Vulkan::DebugLabelScope scope(
		blitCommandBuffer, displayTransferColor,
		"DisplayTransfer inputAddr 0x%08X outputAddr 0x%08X inputWidth %d outputWidth %d inputHeight %d outputHeight %d", inputAddr, outputAddr,
		inputWidth, outputWidth, inputHeight, outputHeight
	);

	blitCommandBuffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {},
		{
			vk::ImageMemoryBarrier(
				vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eTransferRead, vk::ImageLayout::eShaderReadOnlyOptimal,
				vk::ImageLayout::eTransferSrcOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, srcFramebuffer.image.get(),
				vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
			),
			vk::ImageMemoryBarrier(
				vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eTransferWrite, vk::ImageLayout::eShaderReadOnlyOptimal,
				vk::ImageLayout::eTransferDstOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, destFramebuffer.image.get(),
				vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
			),
		}
	);

	blitCommandBuffer.blitImage(
		srcFramebuffer.image.get(), vk::ImageLayout::eTransferSrcOptimal, destFramebuffer.image.get(), vk::ImageLayout::eTransferDstOptimal,
		{blitRegion}, vk::Filter::eLinear
	);

	blitCommandBuffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllGraphics, vk::DependencyFlags(), {}, {},
		{
			vk::ImageMemoryBarrier(
				vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eColorAttachmentWrite, vk::ImageLayout::eTransferSrcOptimal,
				vk::ImageLayout::eShaderReadOnlyOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, srcFramebuffer.image.get(),
				vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
			),
			vk::ImageMemoryBarrier(
				vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eColorAttachmentWrite, vk::ImageLayout::eTransferDstOptimal,
				vk::ImageLayout::eShaderReadOnlyOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, destFramebuffer.image.get(),
				vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
			),
		}
	);
}

void RendererVK::textureCopy(u32 inputAddr, u32 outputAddr, u32 totalBytes, u32 inputSize, u32 outputSize, u32 flags) {}

void RendererVK::drawVertices(PICA::PrimType primType, std::span<const PICA::Vertex> vertices) {
	using namespace Helpers;

	const u32 depthControl = regs[PICA::InternalRegs::DepthAndColorMask];
	const bool depthTestEnable = depthControl & 1;
	const bool depthWriteEnable = getBit<12>(depthControl);
	const int depthFunc = getBits<4, 3>(depthControl);
	const vk::ColorComponentFlags colorMask = vk::ColorComponentFlags(getBits<8, 4>(depthControl));

	const vk::RenderPass curRenderPass = getRenderPass(colourBufferFormat, depthTestEnable ? std::make_optional(depthBufferFormat) : std::nullopt);

	// Create framebuffer, find a way to cache this!
	vk::Framebuffer curFramebuffer = {};
	{
		std::vector<vk::ImageView> renderTargets;

		const auto& colorTexture = getColorRenderTexture(colourBufferLoc, colourBufferFormat, fbSize[0], fbSize[1]);
		renderTargets.emplace_back(colorTexture.imageView.get());

		if (depthTestEnable) {
			const auto& depthTexture = getDepthRenderTexture(depthBufferLoc, depthBufferFormat, fbSize[0], fbSize[1]);
			renderTargets.emplace_back(depthTexture.imageView.get());
		}

		vk::FramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.setRenderPass(curRenderPass);
		framebufferInfo.setAttachments(renderTargets);
		framebufferInfo.setWidth(fbSize[0]);
		framebufferInfo.setHeight(fbSize[1]);
		framebufferInfo.setLayers(1);
		if (auto createResult = device->createFramebufferUnique(framebufferInfo); createResult.result == vk::Result::eSuccess) {
			curFramebuffer = (frameFramebuffers[frameBufferingIndex].emplace_back(std::move(createResult.value))).get();
		} else {
			Helpers::panic("Error creating render-texture framebuffer: %s\n", vk::to_string(createResult.result).c_str());
		}
	}

	vk::RenderPassBeginInfo renderBeginInfo = {};
	renderBeginInfo.renderPass = curRenderPass;
	static const vk::ClearValue ClearColors[] = {
		vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}),
		vk::ClearDepthStencilValue(1.0f, 0),
		vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}),
	};
	renderBeginInfo.pClearValues = ClearColors;
	renderBeginInfo.clearValueCount = std::size(ClearColors);
	renderBeginInfo.renderArea.extent.width = fbSize[0];
	renderBeginInfo.renderArea.extent.height = fbSize[1];
	renderBeginInfo.framebuffer = curFramebuffer;

	const vk::CommandBuffer& commandBuffer = getCurrentCommandBuffer();

	// Todo: Rather than starting a new renderpass for each draw, do some state-tracking to re-use render-passes
	commandBuffer.beginRenderPass(renderBeginInfo, vk::SubpassContents::eInline);
	static const std::array<float, 4> labelColor = {{1.0f, 0.0f, 0.0f, 1.0f}};
	Vulkan::insertDebugLabel(commandBuffer, labelColor, "DrawVertices: %u vertices", vertices.size());
	commandBuffer.endRenderPass();
}

void RendererVK::screenshot(const std::string& name) {}
