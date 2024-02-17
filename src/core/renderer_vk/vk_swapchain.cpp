#include "renderer_vk/vk_swapchain.hpp"
#include "renderer_vk/vk_instance.hpp"
#include "renderer_vk/vk_render_manager.hpp"
#include "renderer_vk/vk_swapchain.hpp"
#include "renderer_vk/vk_scheduler.hpp"
#include "SDL_vulkan.h"

namespace Vulkan {

Swapchain::Swapchain(const Instance& instance_, Scheduler& scheduler_, u32 width, u32 height, SDL_Window* window)
    : instance(instance_), scheduler(scheduler_) {
    if (VkSurfaceKHR newSurface; SDL_Vulkan_CreateSurface(window, instance.getInstance(), &newSurface)) {
        surface = newSurface;
    } else {
        Helpers::warn("Error creating Vulkan surface");
    }
    findPresentFormat();
    queryPresentMode();
    create(width, height);
}

Swapchain::~Swapchain() {
    destroy();
}

void Swapchain::create(u32 width_, u32 height_) {
    width = width_;
    height = height_;
    needsRecreation = false;

    destroy();

    queryPresentMode();
    setSurfaceProperties();

    const u32 familyIndex = instance.getQueueFamilyIndex();
    const vk::SwapchainCreateInfoKHR swapchainInfo = {
        .surface = surface,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .queueFamilyIndexCount = 1u,
        .pQueueFamilyIndices = &familyIndex,
        .preTransform = transform,
        .compositeAlpha = compositeAlpha,
        .presentMode = presentMode,
        .clipped = true,
        .oldSwapchain = nullptr,
    };

    auto createResult = instance.getDevice().createSwapchainKHRUnique(swapchainInfo);
    if (createResult.result == vk::Result::eSuccess) {
        swapchain = std::move(createResult.value);
    } else {
        Helpers::warn("Swapchain creation failed: %d\n", createResult.result);
        return;
    }

    setupImages();
    refreshSemaphores();
}

bool Swapchain::acquireNextImage() {
	scheduler.waitFor(resourceTicks[frameIndex]);
	resourceTicks[frameIndex] = scheduler.getCpuCounter();

    auto device = instance.getDevice();
    const auto semaphore = getImageAcquiredSemaphore();
    const vk::Result result =
        device.acquireNextImageKHR(*swapchain, std::numeric_limits<u64>::max(),
                                   semaphore, VK_NULL_HANDLE, &imageIndex);
    switch (result) {
    case vk::Result::eSuccess:
        break;
    case vk::Result::eSuboptimalKHR:
    case vk::Result::eErrorSurfaceLostKHR:
    case vk::Result::eErrorOutOfDateKHR:
        needsRecreation = true;
        break;
    default:
        Helpers::panic("Swapchain acquire returned unknown result %d\n", result);
        break;
    }

    return !needsRecreation;
}

void Swapchain::present() {
    if (needsRecreation) [[unlikely]] {
        return;
    }

    const auto vk_swapchain = *swapchain;
    const auto waitSemaphore = getPresentReadySemaphore();
    const vk::PresentInfoKHR presentInfo = {
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &waitSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &vk_swapchain,
        .pImageIndices = &imageIndex,
    };

    const vk::Queue queue = instance.getQueue();
    const vk::Result result = queue.presentKHR(presentInfo);

    switch (result) {
    case vk::Result::eSuccess:
        break;
    case vk::Result::eErrorOutOfDateKHR:
    case vk::Result::eErrorSurfaceLostKHR:
        needsRecreation = true;
        break;
    default:
        Helpers::panic("Swapchain presentation failed %d\n", result);
        break;
    }

    // Advance frame index
    frameIndex = (frameIndex + 1) % imageCount;
}

void Swapchain::findPresentFormat() {
    const auto [result, formats] = instance.getPhysicalDevice().getSurfaceFormatsKHR(surface);

    // If there is a single undefined surface format, the device doesn't care, so we'll just use RGBA.
    if (formats[0].format == vk::Format::eUndefined) {
        surfaceFormat.format = vk::Format::eR8G8B8A8Unorm;
        surfaceFormat.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
    } else {
        // Try to find a suitable format.
        for (const vk::SurfaceFormatKHR& sformat : formats) {
            const vk::Format format = sformat.format;
            if (format != vk::Format::eR8G8B8A8Unorm && format != vk::Format::eB8G8R8A8Unorm) {
                continue;
            }

            surfaceFormat.format = format;
            surfaceFormat.colorSpace = sformat.colorSpace;
            break;
        }
    }

    if (surfaceFormat.format == vk::Format::eUndefined) {
        Helpers::panic("Unable to find required swapchain format\n");
        return;
    }

    // Now that we found a suitable format, create the present renderpass for it
    presentRenderPass = RenderManager::createRenderPass(instance.getDevice(), surfaceFormat.format,
                                                        vk::Format::eUndefined, true);
}

void Swapchain::queryPresentMode() {
    const auto [result, presentModes] = instance.getPhysicalDevice().getSurfacePresentModesKHR(surface);
    if (result != vk::Result::eSuccess) [[unlikely]] {
        Helpers::panic("Error enumerating surface present modes: %s\n", vk::to_string(result).c_str());
        return;
    }

    // Fifo support is required by all vulkan implementations, waits for vsync
    presentMode = vk::PresentModeKHR::eFifo;

    // Use mailbox if available, lowest-latency vsync-enabled mode
    if (std::find(presentModes.begin(), presentModes.end(), vk::PresentModeKHR::eMailbox) != presentModes.end()) {
        presentMode = vk::PresentModeKHR::eMailbox;
    }
}

void Swapchain::setSurfaceProperties() {
    const auto [result, capabilities] = instance.getPhysicalDevice().getSurfaceCapabilitiesKHR(surface);
    if (result != vk::Result::eSuccess) [[unlikely]] {
        Helpers::panic("Unable to query surface capabilities\n");
        return;
    }

    extent = capabilities.currentExtent;
    if (capabilities.currentExtent.width == std::numeric_limits<u32>::max()) {
        extent.width = std::max(capabilities.minImageExtent.width,
                                std::min(capabilities.maxImageExtent.width, width));
        extent.height = std::max(capabilities.minImageExtent.height,
                                 std::min(capabilities.maxImageExtent.height, height));
    }

    // Select number of images in swap chain, we prefer one buffer in the background to work on
    imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0) {
        imageCount = std::min(imageCount, capabilities.maxImageCount);
    }

    // Prefer identity transform if possible
    transform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
    if (!(capabilities.supportedTransforms & transform)) {
        transform = capabilities.currentTransform;
    }

    // Opaque is not supported everywhere.
    compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    if (!(capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eOpaque)) {
        compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eInherit;
    }
}

void Swapchain::destroy() {
    swapchain.reset();
    imageViews.clear();
    framebuffers.clear();
    imageAcquired.clear();
    presentReady.clear();
}

void Swapchain::refreshSemaphores() {
    const auto device = instance.getDevice();
    imageAcquired.resize(imageCount);
    presentReady.resize(imageCount);
    resourceTicks.resize(imageCount);
    for (u32 i = 0; i < imageCount; ++i) {
        const vk::SemaphoreCreateInfo semaphoreInfo{};
        imageAcquired[i] = device.createSemaphoreUnique(semaphoreInfo).value;
        presentReady[i] = device.createSemaphoreUnique(semaphoreInfo).value;
    }
}

void Swapchain::setupImages() {
    const auto device = instance.getDevice();

    vk::Result result;
    std::tie(result, images) = device.getSwapchainImagesKHR(*swapchain);
    if (result != vk::Result::eSuccess) {
        Helpers::panic("Unable to setup swapchain images\n");
    }

    imageCount = static_cast<u32>(images.size());
    imageViews.resize(imageCount);
    framebuffers.resize(imageCount);

    // Create image-views and framebuffers
    for (u32 i = 0; i < imageCount; i++) {
        const vk::ImageViewCreateInfo viewInfo = {
            .image = images[i],
            .viewType = vk::ImageViewType::e2D,
            .format = surfaceFormat.format,
            .components = {},
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        if (auto createResult = device.createImageViewUnique(viewInfo); createResult.result == vk::Result::eSuccess) {
            imageViews[i] = std::move(createResult.value);
        } else {
            Helpers::panic("Error creating swapchain image-view: #%zu %s\n", i, vk::to_string(createResult.result).c_str());
        }

        const vk::ImageView view = *imageViews[i];
        const vk::FramebufferCreateInfo fbInfo = {
            .renderPass = getRenderPass(),
            .attachmentCount = 1u,
            .pAttachments = &view,
            .width = extent.width,
            .height = extent.height,
            .layers = 1u,
        };
        if (auto createResult = device.createFramebufferUnique(fbInfo); createResult.result == vk::Result::eSuccess) {
            framebuffers[i] = std::move(createResult.value);
        } else {
            Helpers::panic("Error creating swapchain framebuffer: #%zu %s\n", i, vk::to_string(createResult.result).c_str());
        }
    }
}

} // namespace Vulkan
