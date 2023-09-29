#pragma once

#include "vk_api.hpp"
#include "helpers.hpp"

struct SDL_Window;

namespace Vulkan {

class Instance;
class Scheduler;

class Swapchain {
    const Instance& instance;
    Scheduler& scheduler;
    vk::UniqueSwapchainKHR swapchain{};
    vk::SurfaceKHR surface{};
    vk::SurfaceFormatKHR surfaceFormat;
    vk::PresentModeKHR presentMode;
    vk::Extent2D extent;
    vk::SurfaceTransformFlagBitsKHR transform;
    vk::CompositeAlphaFlagBitsKHR compositeAlpha;
    std::vector<vk::Image> images;
    std::vector<vk::UniqueImageView> imageViews;
    std::vector<vk::UniqueFramebuffer> framebuffers;
    std::vector<vk::UniqueSemaphore> imageAcquired;
    std::vector<vk::UniqueSemaphore> presentReady;
    std::vector<u64> resourceTicks;
    vk::UniqueRenderPass presentRenderPass;
    u32 width = 0;
    u32 height = 0;
    u32 imageCount = 0;
    u32 imageIndex = 0;
    u32 frameIndex = 0;
    bool needsRecreation = true;

  public:
    Swapchain(const Instance& instance, Scheduler& scheduler, u32 width, u32 height, SDL_Window* window);
    ~Swapchain();

    void create(u32 width, u32 height);
    bool acquireNextImage();
    void present();

    vk::SurfaceKHR getSurface() const {
        return surface;
    }

    vk::Image getImage() const {
        return images[imageIndex];
    }

    vk::SurfaceFormatKHR getSurfaceFormat() const {
        return surfaceFormat;
    }

    vk::SwapchainKHR getHandle() const {
        return *swapchain;
    }

    vk::ImageView getImageView() const {
        return *imageViews[imageIndex];
    }

    vk::Framebuffer getFramebuffer() const {
        return *framebuffers[imageIndex];
    }

    vk::RenderPass getRenderPass() const {
        return *presentRenderPass;
    }

    u32 getWidth() const {
        return width;
    }

    u32 getHeight() const {
        return height;
    }

    u32 getImageIndex() const {
        return imageIndex;
    }

    u32 getImageCount() const {
        return imageCount;
    }

    vk::Extent2D getExtent() const {
        return extent;
    }

    vk::Semaphore getImageAcquiredSemaphore() const {
        return *imageAcquired[frameIndex];
    }

    vk::Semaphore getPresentReadySemaphore() const {
        return *presentReady[imageIndex];
    }

  private:
    void findPresentFormat();
    void queryPresentMode();
    void setSurfaceProperties();
    void destroy();
    void setupImages();
    void refreshSemaphores();
};

} // namespace Vulkan
