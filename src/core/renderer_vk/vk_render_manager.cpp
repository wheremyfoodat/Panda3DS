#include "renderer_vk/vk_render_manager.hpp"
#include "renderer_vk/vk_instance.hpp"
#include "renderer_vk/vk_scheduler.hpp"
#include "renderer_vk/vk_pica.hpp"
#include "renderer_vk/vk_swapchain.hpp"
#include "PICA/pica_hash.hpp"

namespace Vulkan {

RenderManager::RenderManager(const Instance& instance_, Scheduler& scheduler_)
        : instance(instance_), scheduler(scheduler_) {}

RenderManager::~RenderManager() = default;

void RenderManager::beginRendering(const std::optional<Vulkan::ColourBuffer>& color,
								   const std::optional<Vulkan::DepthBuffer>& depth,
								   Math::Rectangle<u32> renderArea) {
	const auto renderPass = getRenderPass(color ? color->format : PICA::ColorFmt::Invalid,
										  depth ? depth->format : PICA::DepthFmt::Invalid);
	const std::array<vk::ImageView, 2> imageViews = {
		color ? color->imageView : VK_NULL_HANDLE,
		depth ? depth->imageView : VK_NULL_HANDLE,
	};
	if (isRendering && imageViews[0] == currentColorView && imageViews[1] == currentDepthView) {
		return;
	}

	const size_t fbHash = PICAHash::computeHash(imageViews.data(), sizeof(imageViews));
	const auto [it, new_fb] = framebuffers.try_emplace(fbHash);
	if (new_fb) {
		const auto size = color ? color->size : depth->size;
		it->second = createFramebuffer(imageViews[0], imageViews[1], renderPass, size);
	}

	const vk::RenderPassBeginInfo renderBeginInfo = {
		.renderPass = renderPass,
		.framebuffer = it->second.get(),
		.renderArea = {
			.offset{static_cast<s32>(renderArea.left), static_cast<s32>(renderArea.top)},
			.extent{renderArea.getWidth(), renderArea.getHeight()},
		},
	};

	endRendering();
	scheduler.getCmdBuf().beginRenderPass(renderBeginInfo, vk::SubpassContents::eInline);

	currentColorView = imageViews[0];
	currentDepthView = imageViews[1];
	currentRenderPass = renderPass;
	isRendering = true;
}

void RenderManager::beginRendering(Swapchain& swapchain) {
    const auto renderFb = swapchain.getFramebuffer();
    if (isRendering && currentFb == renderFb) [[likely]] {
        return;
    }

    static constexpr std::array<float, 4> clearColor = {{0.0f, 0.0f, 0.0f, 1.0f}};

    vk::ClearValue clearVal{};
    clearVal.color.float32 = clearColor;

    const vk::RenderPassBeginInfo renderBeginInfo = {
        .renderPass = swapchain.getRenderPass(),
        .framebuffer = renderFb,
        .renderArea = {
            .offset = {0, 0},
            .extent = swapchain.getExtent(),
        },
        .clearValueCount = 1u,
        .pClearValues = &clearVal,
    };

    endRendering();
    scheduler.getCmdBuf().beginRenderPass(renderBeginInfo, vk::SubpassContents::eInline);

	currentRenderPass = swapchain.getRenderPass();
	currentColorView = swapchain.getImageView();
	currentDepthView = VK_NULL_HANDLE;
    isRendering = true;
}

void RenderManager::endRendering() {
    if (!isRendering) {
        return;
    }
    isRendering = false;
    scheduler.getCmdBuf().endRenderPass();
}

vk::RenderPass RenderManager::getRenderPass(PICA::ColorFmt colorFmt, PICA::DepthFmt depthFmt) {
    const u32 colorIndex = colorFmt == PICA::ColorFmt::Invalid ? 5 : static_cast<u32>(colorFmt);
    const u32 depthIndex = depthFmt == PICA::DepthFmt::Invalid ? 4 : static_cast<u32>(depthFmt);
    vk::UniqueRenderPass& renderPass = renderPasses[colorIndex][depthIndex];
    if (!renderPass) {
        renderPass = createRenderPass(instance.getDevice(), Vulkan::colorFormatToVulkan(colorFmt),
                                      Vulkan::depthFormatToVulkan(depthFmt),
                                      false);
    }
    return *renderPass;
}

vk::UniqueRenderPass RenderManager::createRenderPass(vk::Device device, vk::Format color,
                                                     vk::Format depth, bool isPresent) {
    u32 numAttachments = 0;
    std::array<vk::AttachmentDescription, 2> attachments;

    bool useColor = false;
    vk::AttachmentReference colorAttachmentRef{};
    bool useDepth = false;
    vk::AttachmentReference depthAttachmentRef{};

    if (color != vk::Format::eUndefined) {
        attachments[numAttachments] = vk::AttachmentDescription{
            .format = color,
            .loadOp = isPresent ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
            .initialLayout = isPresent ? vk::ImageLayout::eUndefined : vk::ImageLayout::eGeneral,
            .finalLayout = isPresent ? vk::ImageLayout::ePresentSrcKHR : vk::ImageLayout::eGeneral,
        };

        colorAttachmentRef = vk::AttachmentReference{
            .attachment = numAttachments++,
            .layout = vk::ImageLayout::eGeneral,
        };

        useColor = true;
    }

    if (depth != vk::Format::eUndefined) {
        attachments[numAttachments] = vk::AttachmentDescription{
            .format = depth,
            .loadOp = vk::AttachmentLoadOp::eLoad,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .stencilLoadOp = vk::AttachmentLoadOp::eLoad,
            .stencilStoreOp = vk::AttachmentStoreOp::eStore,
            .initialLayout = vk::ImageLayout::eGeneral,
            .finalLayout = vk::ImageLayout::eGeneral,
        };

        depthAttachmentRef = vk::AttachmentReference{
            .attachment = numAttachments++,
            .layout = vk::ImageLayout::eGeneral,
        };

        useDepth = true;
    }

    const vk::SubpassDescription subpassDesc = {
        .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
        .inputAttachmentCount = 0,
        .pInputAttachments = nullptr,
        .colorAttachmentCount = useColor ? 1u : 0u,
        .pColorAttachments = &colorAttachmentRef,
        .pResolveAttachments = 0,
        .pDepthStencilAttachment = useDepth ? &depthAttachmentRef : nullptr,
    };

    const vk::RenderPassCreateInfo renderpassInfo = {
        .attachmentCount = numAttachments,
        .pAttachments = attachments.data(),
        .subpassCount = 1,
        .pSubpasses = &subpassDesc,
    };

    return device.createRenderPassUnique(renderpassInfo).value;
}

vk::UniqueFramebuffer RenderManager::createFramebuffer(vk::ImageView colorView, vk::ImageView depthView,
                                                       vk::RenderPass renderPass, Math::uvec2 size) const {
    u32 numAttachments = 0;
    std::array<vk::ImageView, 2> attachments;

    if (colorView) {
        attachments[numAttachments++] = colorView;
    }
    if (depthView) {
        attachments[numAttachments++] = depthView;
    }

    const vk::FramebufferCreateInfo fbInfo = {
        .renderPass = renderPass,
        .attachmentCount = numAttachments,
        .pAttachments = attachments.data(),
        .width = size.x(),
        .height = size.y(),
        .layers = 1u,
    };

    if (auto createResult = instance.getDevice().createFramebufferUnique(fbInfo); createResult.result == vk::Result::eSuccess) {
        return std::move(createResult.value);
    } else {
        Helpers::panic("Error creating framebuffer: %s\n", vk::to_string(createResult.result).c_str());
        return {};
    }
}

} // namespace Vulkan
