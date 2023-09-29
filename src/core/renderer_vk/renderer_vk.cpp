#include "renderer_vk/renderer_vk.hpp"

#include <cmrc/cmrc.hpp>
#include <limits>
#include <span>
#include <unordered_set>

#include "helpers.hpp"
#include "window.hpp"
#include "renderer_vk/vk_debug.hpp"

#include <vulkan/vulkan_format_traits.hpp>
#include <vulkan/vulkan_hash.hpp>

CMRC_DECLARE(RendererVK);

using namespace Floats;
using namespace Helpers;
using PICA::Vertex;

static vk::SamplerCreateInfo sampler2D(vk::Filter magFilter = vk::Filter::eLinear,
									   vk::Filter minFilter = vk::Filter::eLinear,
									   vk::SamplerMipmapMode mipmapMode = vk::SamplerMipmapMode::eLinear,
									   vk::SamplerAddressMode wrapU = vk::SamplerAddressMode::eClampToEdge,
									   vk::SamplerAddressMode wrapT = vk::SamplerAddressMode::eClampToEdge) {
	vk::SamplerCreateInfo samplerInfo = {};
	samplerInfo.magFilter = magFilter;
	samplerInfo.minFilter = minFilter;
	samplerInfo.mipmapMode = mipmapMode;
	samplerInfo.addressModeU = wrapU;
	samplerInfo.addressModeV = wrapT;
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

static constexpr u32 VertexBufferSize = 32 * 1024 * 1024;

RendererVK::RendererVK(GPU& gpu, const Window& window_,
                       const std::array<u32, regNum>& internalRegs,
                       const std::array<u32, extRegNum>& externalRegs)
    : Renderer(gpu, internalRegs, externalRegs), window(window_), instance(static_cast<SDL_Window*>(window.getHandle())),
      scheduler(instance), swapchain(instance, scheduler, 400, 480, static_cast<SDL_Window*>(window.getHandle())), renderManager(instance, scheduler),
      samplerCache(instance.getDevice()),
      defaultTex(0, PICA::TextureFmt::RGBA8, 1, 1, 0), lightingLut(vk::Format::eR32Sfloat, 256, 1, 0),
      uniformBuffer(instance, scheduler, vk::BufferUsageFlagBits::eUniformBuffer, UniformBufferSize),
      vertexBuffer(instance, scheduler, vk::BufferUsageFlagBits::eVertexBuffer, VertexBufferSize),
      uploadBuffer(instance, scheduler, vk::BufferUsageFlagBits::eTransferSrc, 64 * 1024 * 1024),
      pipelineManager(instance, scheduler, uniformBuffer.getHandle()),
      descriptorUpdateBatch(instance.getDevice()) {
    defaultTex.allocate(instance, scheduler);
    lightingLut.allocate(instance, scheduler, vk::ImageType::e1D, vk::ImageViewType::e1DArray, PICA::Lights::LUT_Count);
    auto device = instance.getDevice();

	scheduler.setSubmitCallback([this] {
		renderManager.endRendering();
		pipelineManager.flushUpdates();
	});

	static constexpr std::array<vk::DescriptorSetLayoutBinding, 1> displayShaderLayout = {{
		{0, vk::DescriptorType::eCombinedImageSampler, 2, vk::ShaderStageFlagBits::eFragment},
	}};

	if (auto createResult = Vulkan::DescriptorHeap::create(device, scheduler, displayShaderLayout); createResult.has_value()) {
		displayDescriptorHeap = std::make_unique<Vulkan::DescriptorHeap>(std::move(createResult.value()));
	} else {
		Helpers::panic("Error creating descriptor heap\n");
	}

	for (usize i = 0; i < swapchain.getImageCount(); ++i) {
		if (auto allocateResult = displayDescriptorHeap->allocateDescriptorSet(); allocateResult.has_value()) {
			displayPipelineDescriptorSets.emplace_back(allocateResult.value());
		} else {
			Helpers::panic("Error creating descriptor set\n");
		}
	}

	auto vk_resources = cmrc::RendererVK::get_filesystem();
	auto displayVertexShader = vk_resources.open("vulkan_display.vert.spv");
	auto displayFragmentShader = vk_resources.open("vulkan_display.frag.spv");

	vk::UniqueShaderModule displayVertexShaderModule = Vulkan::createShaderModule(device, displayVertexShader);
	vk::UniqueShaderModule displayFragmentShaderModule = Vulkan::createShaderModule(device, displayFragmentShader);

	const std::array pushRanges = {
		vk::PushConstantRange{
			.stageFlags = vk::ShaderStageFlagBits::eFragment,
			.offset = 0,
			.size = sizeof(u32),
		}
	};
	const std::array setLayouts = {
		displayDescriptorHeap.get()->getDescriptorSetLayout()
	};

	// Create Pipeline Layout
	const vk::PipelineLayoutCreateInfo graphicsPipelineLayoutInfo = {
		.setLayoutCount = setLayouts.size(),
		.pSetLayouts = setLayouts.data(),
		.pushConstantRangeCount = pushRanges.size(),
		.pPushConstantRanges = pushRanges.data(),
	};

	if (auto createResult = device.createPipelineLayoutUnique(graphicsPipelineLayoutInfo); createResult.result == vk::Result::eSuccess) {
		displayPipelineLayout = std::move(createResult.value);
	} else {
		Helpers::panic("Error creating pipeline layout: %s\n", vk::to_string(createResult.result).c_str());
		return;
	}

	Vulkan::PipelineInfo displayInfo{};
	displayInfo.rasterization.topology = vk::PrimitiveTopology::eTriangleStrip;
	displayPipeline = std::make_unique<Vulkan::Pipeline>(
		device, displayPipelineLayout.get(), displayVertexShaderModule.get(),
		displayFragmentShaderModule.get(), std::span<const vk::VertexInputBindingDescription>{},
		std::span<const vk::VertexInputAttributeDescription>{}, displayInfo, swapchain.getRenderPass()
	);

	sampler = samplerCache.getSampler(sampler2D(vk::Filter::eNearest, vk::Filter::eNearest, vk::SamplerMipmapMode::eNearest));
}

RendererVK::~RendererVK() {
	defaultTex.free();
	lightingLut.free();
	reset();
}

void RendererVK::reset() {
	scheduler.finish();
	depthBufferCache.reset();
	colourBufferCache.reset();
	textureCache.reset();
	renderManager.reset();
}

void RendererVK::setupBlending() {
	// Map of PICA blending equations to Vulkan blending equations. The unused blending equations are equivalent to equation 0 (add)
	// TODO: Proper handling of min/max blending factors.
	static constexpr std::array<vk::BlendOp, 8> blendingEquations = {
		vk::BlendOp::eAdd, vk::BlendOp::eSubtract, vk::BlendOp::eReverseSubtract, vk::BlendOp::eMin, vk::BlendOp::eMax,
		vk::BlendOp::eAdd, vk::BlendOp::eAdd, vk::BlendOp::eAdd,
	};

	// Map of PICA blending funcs to Vulkan blending funcs. Func = 15 is undocumented and stubbed to GL_ONE for now
	static constexpr std::array<vk::BlendFactor, 16> blendingFuncs = {
		vk::BlendFactor::eZero,
		vk::BlendFactor::eOne,
		vk::BlendFactor::eSrcColor,
		vk::BlendFactor::eOneMinusSrcColor,
		vk::BlendFactor::eDstColor,
		vk::BlendFactor::eOneMinusDstColor,
		vk::BlendFactor::eSrcAlpha,
		vk::BlendFactor::eOneMinusSrcAlpha,
		vk::BlendFactor::eDstAlpha,
		vk::BlendFactor::eOneMinusDstAlpha,
		vk::BlendFactor::eConstantColor,
		vk::BlendFactor::eOneMinusConstantColor,
		vk::BlendFactor::eConstantAlpha,
		vk::BlendFactor::eOneMinusConstantAlpha,
		vk::BlendFactor::eSrcAlphaSaturate,
		vk::BlendFactor::eOne,
	};

	static constexpr std::array<vk::LogicOp, 16> logicOps = {
		vk::LogicOp::eClear, vk::LogicOp::eAnd, vk::LogicOp::eAndReverse, vk::LogicOp::eCopy, vk::LogicOp::eSet,
		vk::LogicOp::eCopyInverted, vk::LogicOp::eNoOp, vk::LogicOp::eInvert,
		vk::LogicOp::eNand, vk::LogicOp::eOr, vk::LogicOp::eNor, vk::LogicOp::eXor, vk::LogicOp::eEquivalent,
		vk::LogicOp::eAndInverted,  vk::LogicOp::eOrReverse, vk::LogicOp::eOrInverted,
	};

	// Shows if blending is enabled. If it is not enabled, then logic ops are enabled instead
	const bool blendingEnabled = (regs[PICA::InternalRegs::ColourOperation] & (1 << 8)) != 0;
	const u32 logicOp = getBits<0, 4>(regs[PICA::InternalRegs::LogicOp]);

	auto& blending = picaPipelineInfo.blending;
	blending.blendEnable = blendingEnabled;
	blending.logicOp = logicOps[logicOp];

	// Get blending equations
	const u32 blendControl = regs[PICA::InternalRegs::BlendFunc];
	blending.colorBlendEq = blendingEquations[blendControl & 0x7];
	blending.alphaBlendEq = blendingEquations[getBits<8, 3>(blendControl)];

	// Get blending functions
	blending.srcColorBlendFactor = blendingFuncs[getBits<16, 4>(blendControl)];
	blending.dstColorBlendFactor = blendingFuncs[getBits<20, 4>(blendControl)];
	blending.srcAlphaBlendFactor = blendingFuncs[getBits<24, 4>(blendControl)];
	blending.dstAlphaBlendFactor = blendingFuncs[getBits<28, 4>(blendControl)];
	picaPipelineInfo.dynamic.blendColor = regs[PICA::InternalRegs::BlendColour];
}

void RendererVK::setupTopologyAndCulling(PICA::PrimType primType) {
	static constexpr std::array<vk::PrimitiveTopology, 4> primTypes = {
		vk::PrimitiveTopology::eTriangleList,
		vk::PrimitiveTopology::eTriangleStrip,
		vk::PrimitiveTopology::eTriangleFan,
		vk::PrimitiveTopology::eTriangleList,
	};

	// Set topology.
	auto& rasterization = picaPipelineInfo.rasterization;
	rasterization.topology = primTypes[static_cast<usize>(primType)];

	// TODO: Set cull mode.
}

void RendererVK::setupStencilTest() {
	auto& depthStencil = picaPipelineInfo.depthStencil;
	auto& dynamic = picaPipelineInfo.dynamic;

	const u32 stencilConfig = regs[PICA::InternalRegs::StencilTest];
	const bool stencilEnable = getBit<0>(stencilConfig);
	if (!stencilEnable) {
		depthStencil.stencilTestEnable = false;
		return;
	}

	static constexpr std::array<vk::CompareOp, 8> stencilFuncs = {
		vk::CompareOp::eNever,
		vk::CompareOp::eAlways,
		vk::CompareOp::eEqual,
		vk::CompareOp::eNotEqual,
		vk::CompareOp::eLess,
		vk::CompareOp::eLessOrEqual,
		vk::CompareOp::eGreater,
		vk::CompareOp::eGreaterOrEqual,
	};
	depthStencil.stencilTestEnable = true;

	const u32 stencilFunc = getBits<4, 3>(stencilConfig);
	const s32 reference = s8(getBits<16, 8>(stencilConfig)); // Signed reference value
	const u32 stencilRefMask = getBits<24, 8>(stencilConfig);

	const bool stencilWrite = regs[PICA::InternalRegs::DepthBufferWrite];
	const u32 stencilBufferMask = stencilWrite ? getBits<8, 8>(stencilConfig) : 0;

	depthStencil.stencilCompareOp = stencilFuncs[stencilFunc];
	dynamic.stencilReference = reference;
	dynamic.stencilCompareMask = stencilRefMask;
	dynamic.stencilWriteMask = stencilBufferMask;

	static constexpr std::array<vk::StencilOp, 8> stencilOps = {
		vk::StencilOp::eKeep,
		vk::StencilOp::eZero,
		vk::StencilOp::eReplace,
		vk::StencilOp::eIncrementAndClamp,
		vk::StencilOp::eDecrementAndClamp,
		vk::StencilOp::eInvert,
		vk::StencilOp::eIncrementAndWrap,
		vk::StencilOp::eDecrementAndWrap,
	};
	const u32 stencilOpConfig = regs[PICA::InternalRegs::StencilOp];
	const u32 stencilFailOp = getBits<0, 3>(stencilOpConfig);
	const u32 depthFailOp = getBits<4, 3>(stencilOpConfig);
	const u32 passOp = getBits<8, 3>(stencilOpConfig);

	depthStencil.stencilFailOp = stencilOps[stencilFailOp];
	depthStencil.stencilPassOp = stencilOps[passOp];
	depthStencil.stencilDepthFailOp = stencilOps[depthFailOp];
}

void RendererVK::bindTexturesToSlots() {
	static constexpr std::array<u32, 3> ioBases = {
		PICA::InternalRegs::Tex0BorderColor,
		PICA::InternalRegs::Tex1BorderColor,
		PICA::InternalRegs::Tex2BorderColor,
	};

	pipelineManager.beginUpdate();

	for (int i = 0; i < 3; i++) {
		if ((regs[PICA::InternalRegs::TexUnitCfg] & (1 << i)) == 0) {
			pipelineManager.pushImage(defaultTex.imageView, sampler);
			continue;
		}

		const size_t ioBase = ioBases[i];

		const u32 dim = regs[ioBase + 1];
		const u32 config = regs[ioBase + 2];
		const u32 height = dim & 0x7ff;
		const u32 width = getBits<16, 11>(dim);
		const u32 addr = (regs[ioBase + 4] & 0x0FFFFFFF) << 3;
		u32 format = regs[ioBase + (i == 0 ? 13 : 5)] & 0xF;

		// The wrapping mode field is 3 bits instead of 2 bits.
		// The bottom 4 undocumented wrapping modes are taken from Citra.
		static constexpr std::array<vk::SamplerAddressMode, 8> wrappingModes = {
			vk::SamplerAddressMode::eClampToEdge, vk::SamplerAddressMode::eClampToBorder, vk::SamplerAddressMode::eRepeat,
			vk::SamplerAddressMode::eMirroredRepeat, vk::SamplerAddressMode::eClampToEdge, vk::SamplerAddressMode::eClampToBorder,
			vk::SamplerAddressMode::eRepeat, vk::SamplerAddressMode::eRepeat
		};

		const auto magFilter = (config & 0x2) != 0 ? vk::Filter::eLinear : vk::Filter::eNearest;
		const auto minFilter = (config & 0x4) != 0 ? vk::Filter::eLinear : vk::Filter::eNearest;
		const auto wrapT = wrappingModes[getBits<8, 3>(config)];
		const auto wrapS = wrappingModes[getBits<12, 3>(config)];
		const auto textureSampler = samplerCache.getSampler(sampler2D(magFilter, minFilter, vk::SamplerMipmapMode::eLinear, wrapT, wrapS));

		if (addr != 0) [[likely]] {
			Vulkan::Texture targetTex(addr, static_cast<PICA::TextureFmt>(format), width, height, config);
			vk::ImageView imageView = getTexture(targetTex);
			pipelineManager.pushImage(imageView, textureSampler);
		} else {
			// Mapping a texture from NULL. PICA seems to read the last sampled colour, but for now we will display a black texture instead since it is far easier.
			// Games that do this don't really care what it does, they just expect the PICA to not crash, since it doesn't have a PU/MMU and can do all sorts of
			// Weird invalid memory accesses without crashing
			pipelineManager.pushImage(defaultTex.imageView, textureSampler);
		}
	}

	pipelineManager.pushImage(lightingLut.imageView, sampler);
	pipelineManager.flushUpdates();
}

vk::ImageView RendererVK::getTexture(Vulkan::Texture& tex) {
	// Similar logic as the getColourFBO/bindDepthBuffer functions
	auto buffer = textureCache.find(tex);

	if (buffer.has_value()) {
		return buffer.value().get().imageView;
	} else {
		const auto textureData = std::span{gpu.getPointerPhys<u8>(tex.location), tex.sizeInBytes()};  // Get pointer to the texture data in 3DS memory
		renderManager.endRendering();
		Vulkan::Texture& newTex = textureCache.add(tex, instance, scheduler);
		renderManager.endRendering();
		std::vector<u32> decoded = newTex.decodeTexture(textureData);
		const auto data = std::span{(u8*)decoded.data(), decoded.size() * sizeof(u32)};
		newTex.uploadTexture(data, 1, uploadBuffer);

		return newTex.imageView;
	}
}

void RendererVK::uploadUniforms() {
	const VsSupportBuffer vsSupport{regs};
	const FsSupportBuffer fsSupport{regs};

	// Map space for both vertex and fragment uniforms and copy them.
	auto [data, offset] = uniformBuffer.Map(UniformSize);
	std::memcpy(data.data(), &vsSupport, sizeof(VsSupportBuffer));
	std::memcpy(data.data() + sizeof(VsSupportBuffer), &fsSupport, sizeof(FsSupportBuffer));

	// Update the ranges in the pipeline manager for when we bind the descriptor sets.
	pipelineManager.updateRange(Vulkan::BufferKind::VsSupport, offset);
	pipelineManager.updateRange(Vulkan::BufferKind::FsSupport, offset + sizeof(VsSupportBuffer));
}

void RendererVK::updateLightingLUT() {
	gpu.lightingLUTDirty = false;
	std::array<float, GPU::LightingLutSize> u16_lightinglut;

	for (int i = 0; i < gpu.lightingLUT.size(); i++) {
		uint64_t value = gpu.lightingLUT[i] & ((1 << 12) - 1);
		u16_lightinglut[i] = value / 4095.f;
	}

	// Upload to the texture.
	const auto data = std::span{(u8*)u16_lightinglut.data(), u16_lightinglut.size() * sizeof(float)};
	renderManager.endRendering();
	lightingLut.uploadTexture(data, PICA::Lights::LUT_Count, uploadBuffer);
}

void RendererVK::drawScreen(u32 screenIndex, vk::Rect2D drawArea) {
    const vk::CommandBuffer cmdbuf = scheduler.getCmdBuf();
    cmdbuf.setViewport(0, vk::Viewport(drawArea.offset.x, drawArea.offset.y, drawArea.extent.width, drawArea.extent.height));
    cmdbuf.setScissor(0, drawArea);
    cmdbuf.pushConstants(displayPipelineLayout.get(), vk::ShaderStageFlagBits::eFragment,
                         0, sizeof(u32), &screenIndex);
    cmdbuf.draw(4, 1, 0, 0);
}

void RendererVK::display() {
    const bool topActiveFb = externalRegs[PICA::ExternalRegs::Framebuffer0Select] & 1;
    const u32 topScreenAddr = externalRegs[topActiveFb ? PICA::ExternalRegs::Framebuffer0AFirstAddr
                                                       : PICA::ExternalRegs::Framebuffer0ASecondAddr];

    const bool bottomActiveFb = externalRegs[PICA::ExternalRegs::Framebuffer1Select] & 1;
    const u32 bottomScreenAddr =
        externalRegs[bottomActiveFb ? PICA::ExternalRegs::Framebuffer1AFirstAddr
                                    : PICA::ExternalRegs::Framebuffer1ASecondAddr];

    auto topScreen = colourBufferCache.findFromAddress(topScreenAddr);
    auto bottomScreen = colourBufferCache.findFromAddress(bottomScreenAddr);
    if (!topScreen && !bottomScreen) {
        return; // Nothing to display :(
    }

    while (!swapchain.acquireNextImage()) {
        scheduler.finish();
        swapchain.create(window.getWidth(), window.getHeight());
    }

    // Write the present descriptor set
    const auto targetSet = displayPipelineDescriptorSets[swapchain.getImageIndex()];
    descriptorUpdateBatch.addImageSampler(targetSet, 0, 0, topScreen ? topScreen->get().imageView : defaultTex.imageView, sampler);
    descriptorUpdateBatch.addImageSampler(targetSet, 0, 1, bottomScreen ? bottomScreen->get().imageView : defaultTex.imageView, sampler);
    descriptorUpdateBatch.flush();

    // Bind the present pipeline
    const vk::CommandBuffer cmdbuf = scheduler.getCmdBuf();
    cmdbuf.bindPipeline(vk::PipelineBindPoint::eGraphics, displayPipeline->getPipeline());
    cmdbuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, displayPipelineLayout.get(), 0, targetSet, {});

    // Draw the screens to the swapchain
    renderManager.beginRendering(swapchain);
    {
        static const std::array<float, 4> renderScreenScopeColor = {{1.0f, 0.0f, 1.0f, 1.0f}};
        const Vulkan::DebugLabelScope debugScope(cmdbuf, renderScreenScopeColor, "Render Screen");

        if (topScreen) {
            const vk::Rect2D topArea{{0, 0}, {400, 240}};
            drawScreen(0, topArea);
        }
        if (bottomScreen) {
            const vk::Rect2D bottomArea{{40, 240}, {320, 240}};
            drawScreen(1, bottomArea);
        }
    }
    renderManager.endRendering();

    const auto signalSemaphore = swapchain.getPresentReadySemaphore();
    const auto waitSemaphore = swapchain.getImageAcquiredSemaphore();
    pipelineManager.flushUpdates();
    scheduler.submitWork(signalSemaphore, waitSemaphore);
    swapchain.present();
}

void RendererVK::clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) {
	vk::Format vkFormat{};
	vk::Image image{};
	auto renderTexture = colourBufferCache.findFromAddress(startAddress);
	if (!renderTexture) {
		const auto depthTexture = depthBufferCache.findFromAddress(startAddress);
		if (depthTexture) {
			vkFormat = depthTexture->get().vkFormat;
			image = depthTexture->get().image;
		} else {
			// not found
			return;
		}
	} else {
		vkFormat = renderTexture->get().vkFormat;
		image = renderTexture->get().image;
	}

    const bool isColor = *vk::componentName(vkFormat, 0) != 'D';
    const vk::ImageAspectFlags aspect = isColor ? vk::ImageAspectFlagBits::eColor
                                                : (vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);
    const vk::CommandBuffer cmdbuf = scheduler.getCmdBuf();

    const vk::ImageSubresourceRange range = {
        .aspectMask = aspect,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };

    vk::ImageMemoryBarrier preBarrier = {
        .srcAccessMask = vk::AccessFlagBits::eShaderRead,
        .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
        .oldLayout = vk::ImageLayout::eGeneral,
        .newLayout = vk::ImageLayout::eTransferDstOptimal,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = range,
    };

    vk::ImageMemoryBarrier postBarrier = {
        .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
        .dstAccessMask = vk::AccessFlagBits::eShaderRead,
        .oldLayout = vk::ImageLayout::eTransferDstOptimal,
        .newLayout = vk::ImageLayout::eGeneral,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = range,
    };

    if (isColor) {
		// Color-Clear
		vk::ClearColorValue clearColor = {};
		clearColor.float32[0] = Helpers::getBits<24, 8>(value) / 255.0f;  // r
		clearColor.float32[1] = Helpers::getBits<16, 8>(value) / 255.0f;  // g
		clearColor.float32[2] = Helpers::getBits<8, 8>(value) / 255.0f;   // b
		clearColor.float32[3] = Helpers::getBits<0, 8>(value) / 255.0f;   // a

        const Vulkan::DebugLabelScope scope(
            cmdbuf, clearColor.float32, "ClearBuffer start:%08X end:%08X value:%08X control:%08X\n", startAddress, endAddress,
			value, control
		);

        cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer,
                               vk::DependencyFlagBits::eByRegion, {}, {}, preBarrier);

        cmdbuf.clearColorImage(image, vk::ImageLayout::eTransferDstOptimal, clearColor, range);

        cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllGraphics,
                               vk::DependencyFlagBits::eByRegion, {}, {}, postBarrier);
	} else {
		// Depth-Clear
		vk::ClearDepthStencilValue clearDepthStencil = {};

        if (vk::componentBits(vkFormat, 0) == 16) {
			clearDepthStencil.depth = (value & 0xffff) / 65535.0f;
		} else {
			clearDepthStencil.depth = (value & 0xffffff) / 16777215.0f;
		}

		clearDepthStencil.stencil = (value >> 24);  // Stencil

		const std::array<float, 4> scopeColor = {{clearDepthStencil.depth, clearDepthStencil.depth, clearDepthStencil.depth, 1.0f}};
        const Vulkan::DebugLabelScope scope(
            scheduler.getCmdBuf(), scopeColor, "ClearBuffer start:%08X end:%08X value:%08X control:%08X\n", startAddress, endAddress, value,
			control
		);

        cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer,
                               vk::DependencyFlagBits::eByRegion, {}, {}, preBarrier);

        static constexpr std::array depthStencilRanges = {
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1),
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eStencil, 0, 1, 0, 1)};

        cmdbuf.clearDepthStencilImage(image, vk::ImageLayout::eTransferDstOptimal,
                                      &clearDepthStencil, vk::componentCount(vkFormat),
                                      depthStencilRanges.data());

        cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllGraphics,
                               vk::DependencyFlagBits::eByRegion, {}, {}, postBarrier);
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

    auto srcFramebuffer = getColourBuffer(inputAddr, inputFormat, inputWidth, outputHeight);
    Math::Rect<u32> srcRect = srcFramebuffer->getSubRect(inputAddr, outputWidth, outputHeight);

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

    auto destFramebuffer = getColourBuffer(outputAddr, outputFormat, outputWidth, outputHeight);
    Math::Rect<u32> destRect = destFramebuffer->getSubRect(outputAddr, outputWidth, outputHeight);

	if (inputWidth != outputWidth) {
		// Helpers::warn("Strided display transfer is not handled correctly!\n");
	}

	renderManager.endRendering();

    const std::array<vk::Offset3D, 2> srcOffsets = {{
        {static_cast<s32>(srcRect.left), static_cast<s32>(srcRect.top), 0},
        {static_cast<s32>(srcRect.right), static_cast<s32>(srcRect.bottom), 1}
    }};
    const std::array<vk::Offset3D, 2> dstOffsets = {{
        {static_cast<s32>(destRect.left), static_cast<s32>(destRect.top), 0},
        {static_cast<s32>(destRect.right), static_cast<s32>(destRect.bottom), 1}
    }};
    const vk::ImageBlit blitRegion = {
        .srcSubresource = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .srcOffsets = srcOffsets,
        .dstSubresource = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .dstOffsets = dstOffsets,
    };

    const vk::CommandBuffer cmdbuf = scheduler.getCmdBuf();

	static const std::array<float, 4> displayTransferColor = {{1.0f, 1.0f, 0.0f, 1.0f}};
    const Vulkan::DebugLabelScope scope(
        cmdbuf, displayTransferColor,
		"DisplayTransfer inputAddr 0x%08X outputAddr 0x%08X inputWidth %d outputWidth %d inputHeight %d outputHeight %d", inputAddr, outputAddr,
		inputWidth, outputWidth, inputHeight, outputHeight
	);

    const vk::ImageSubresourceRange range = {
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };

    const std::array preBarriers = {
        vk::ImageMemoryBarrier{
            .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
            .dstAccessMask = vk::AccessFlagBits::eTransferRead,
            .oldLayout = vk::ImageLayout::eGeneral,
            .newLayout = vk::ImageLayout::eTransferSrcOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = srcFramebuffer->image,
            .subresourceRange = range,
        },
        vk::ImageMemoryBarrier{
            .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
            .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
            .oldLayout = vk::ImageLayout::eGeneral,
            .newLayout = vk::ImageLayout::eTransferDstOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = destFramebuffer->image,
            .subresourceRange = range,
        },
    };

    const std::array postBarriers = {
        vk::ImageMemoryBarrier{
            .srcAccessMask = vk::AccessFlagBits::eTransferRead,
            .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
            .oldLayout = vk::ImageLayout::eTransferSrcOptimal,
            .newLayout = vk::ImageLayout::eGeneral,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = srcFramebuffer->image,
            .subresourceRange = range,
        },
        vk::ImageMemoryBarrier{
            .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
            .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
            .oldLayout = vk::ImageLayout::eTransferDstOptimal,
            .newLayout = vk::ImageLayout::eGeneral,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = destFramebuffer->image,
            .subresourceRange = range,
        },
    };

    cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer,
                           vk::DependencyFlagBits::eByRegion, {}, {}, preBarriers);

    cmdbuf.blitImage(srcFramebuffer->image, vk::ImageLayout::eTransferSrcOptimal,
                     destFramebuffer->image, vk::ImageLayout::eTransferDstOptimal,
                     blitRegion, vk::Filter::eLinear);

    cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllGraphics,
                           vk::DependencyFlagBits::eByRegion, {}, {}, postBarriers);
}

void RendererVK::textureCopy(u32 inputAddr, u32 outputAddr, u32 totalBytes, u32 inputSize, u32 outputSize, u32 flags) {}

void RendererVK::drawVertices(PICA::PrimType primType, std::span<const PICA::Vertex> vertices) {
	// Upload vertices
	const auto [data, offset] = vertexBuffer.Map(vertices.size_bytes());
	std::memcpy(data.data(), vertices.data(), data.size());

	// Retrieve color and depth framebuffers.
	const auto colourBuf = getColourBuffer(colourBufferLoc, colourBufferFormat, fbSize[0], fbSize[1]);
	const auto depthBuf = getDepthBuffer();

	// Configure viewport.
	auto& viewport = picaPipelineInfo.dynamic.viewport;
	viewport.x = float(regs[PICA::InternalRegs::ViewportXY] & 0x3ff);
	viewport.y = float((regs[PICA::InternalRegs::ViewportXY] >> 16) & 0x3ff);
	viewport.width = float(f24::fromRaw(regs[PICA::InternalRegs::ViewportWidth] & 0xffffff).toFloat32() * 2.0f);
	viewport.height = float(f24::fromRaw(regs[PICA::InternalRegs::ViewportHeight] & 0xffffff).toFloat32() * 2.0f);
	viewport.maxDepth = 1.f;
	const auto rect = colourBuf->getSubRect(colourBufferLoc, fbSize[0], fbSize[1]);

	// Configure scissor
	auto& scissor = picaPipelineInfo.dynamic.scissor;
	scissor.offset.x = rect.left;
	scissor.offset.y = rect.top;
	scissor.extent.width = rect.getWidth();
	scissor.extent.height = rect.getHeight();

	// Refresh Vulkan pipeline information from PICA state.
	setupTopologyAndCulling(primType);
	setupBlending();
	setupStencilTest();
	uploadUniforms();
	bindTexturesToSlots();

	// If the lighting lut is dirty, re-upload.
	if (gpu.lightingLUTDirty) {
		updateLightingLUT();
	}

	// Begin the renderpass
	renderManager.beginRendering(colourBuf, depthBuf, rect);
	picaPipelineInfo.renderPass = renderManager.getCurrentRenderPass();

	// Bind our generated vulkan pipeline.
	pipelineManager.bindPipeline(picaPipelineInfo);

	// We are ready to draw!
	const auto cmdbuf = scheduler.getCmdBuf();
	cmdbuf.bindVertexBuffers(0, vertexBuffer.getHandle(), offset);
	cmdbuf.draw(vertices.size(), 1, 0, 0);
}

std::optional<Vulkan::ColourBuffer> RendererVK::getColourBuffer(u32 addr, PICA::ColorFmt format, u32 width, u32 height, bool createIfnotFound) {
    // Try to find an already existing buffer that contains the provided address
    // This is a more relaxed check compared to getColourFBO as display transfer/texcopy may refer to
    // subrect of a surface and in case of texcopy we don't know the format of the surface.
    auto buffer = colourBufferCache.findFromAddress(addr);
    if (buffer.has_value()) {
        return buffer.value().get();
    }

    if (!createIfnotFound) {
        return std::nullopt;
    }

    // Otherwise create and cache a new buffer.
    Vulkan::ColourBuffer sampleBuffer(addr, format, width, height);
    renderManager.endRendering();
    return colourBufferCache.add(sampleBuffer, instance, scheduler);
}

std::optional<Vulkan::DepthBuffer> RendererVK::getDepthBuffer() {
	// Note: The code below must execute after we've bound the colour buffer & its framebuffer
	// Because it attaches a depth texture to the aforementioned colour buffer
	const u32 depthControl = regs[PICA::InternalRegs::DepthAndColorMask];
	const bool depthWrite = regs[PICA::InternalRegs::DepthBufferWrite];
	const bool depthEnable = depthControl & 1;
	const bool depthWriteEnable = Helpers::getBit<12>(depthControl);
	const int depthFunc = Helpers::getBits<4, 3>(depthControl);

	const u32 stencilConfig = regs[PICA::InternalRegs::StencilTest];
	const bool stencilEnable = Helpers::getBit<0>(stencilConfig);

	// Update color mask.
	const u32 colourMask = getBits<8, 4>(depthControl);
	picaPipelineInfo.blending.colorWriteMask = colourMask;

	static constexpr std::array<vk::CompareOp, 8> depthModes = {
		vk::CompareOp::eNever,
		vk::CompareOp::eAlways,
		vk::CompareOp::eEqual,
		vk::CompareOp::eNotEqual,
		vk::CompareOp::eLess,
		vk::CompareOp::eLessOrEqual,
		vk::CompareOp::eGreater,
		vk::CompareOp::eGreaterOrEqual,
	};

	const auto getBuffer = [&]() -> std::optional<Vulkan::DepthBuffer> {
		Vulkan::DepthBuffer sampleBuffer(depthBufferLoc, depthBufferFormat, fbSize[0], fbSize[1]);
		auto buffer = depthBufferCache.find(sampleBuffer);
		if (buffer.has_value()) {
			return buffer;
		} else {
			renderManager.endRendering();
			return depthBufferCache.add(sampleBuffer, instance, scheduler);
		}
	};

	auto& depthStencil = picaPipelineInfo.depthStencil;
	if (depthEnable) {
		depthStencil.depthTestEnable = true;
		depthStencil.depthWriteEnable = depthWriteEnable && depthWrite;
		depthStencil.depthCompareOp = depthModes[depthFunc];
		return getBuffer();
	} else {
		if (depthWriteEnable) {
			depthStencil.depthTestEnable = true;
			depthStencil.depthWriteEnable = true;
			depthStencil.depthCompareOp = vk::CompareOp::eAlways;
			return getBuffer();
		} else {
			depthStencil.depthTestEnable = false;
			if (stencilEnable) {
				return getBuffer();
			}
		}
	}

	return std::nullopt;
}

void RendererVK::screenshot(const std::string& name) {}
