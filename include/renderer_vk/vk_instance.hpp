#pragma once

#include "vk_api.hpp"
#include "helpers.hpp"

struct SDL_Window;

VK_DEFINE_HANDLE(VmaAllocator)

namespace Vulkan {

class Instance {
    vk::DynamicLoader dl;
    vk::UniqueInstance instance{};
    vk::UniqueDebugUtilsMessengerEXT debugMessenger{};
    vk::PhysicalDevice physicalDevice{};
    vk::UniqueDevice device{};
    vk::PhysicalDeviceProperties properties{};
    vk::PhysicalDeviceFeatures features{};
    VmaAllocator allocator;
    vk::Queue graphicsQueue{};
    SDL_Window* window;
    bool enableDebug;
    bool timelineSemaphores{};
    bool portabilitySubset{};
    bool customBorderColor{};
    bool indexTypeUint8{};
    int queueFamilyIndex{-1};

  public:
    Instance(SDL_Window* window, u32 physicalDevice = 0, bool enableDebug = true);
    ~Instance();

    vk::Instance getInstance() const noexcept {
        return *instance;
    }

    VmaAllocator getAllocator() const noexcept {
        return allocator;
    }

    vk::PhysicalDevice getPhysicalDevice() const noexcept {
        return physicalDevice;
    }

    vk::Device getDevice() const noexcept {
        return *device;
    }

    vk::Queue getQueue() const noexcept {
        return graphicsQueue;
    }

    u32 getQueueFamilyIndex() const noexcept {
        return queueFamilyIndex;
    }

    bool hasTimelineSemaphores() const noexcept {
        return timelineSemaphores;
    }

    bool hasPortabilitySubset() const noexcept {
        return portabilitySubset;
    }

    bool hasCustomBorderColor() const noexcept {
        return customBorderColor;
    }

    bool hasIndexTypeUint8() const noexcept {
        return indexTypeUint8;
    }

  private:
    void createInstance();
    void pickPhysicalDevice(u32 index);
    void pickGraphicsQueue();
    void createDevice();
    void createAllocator();

    std::vector<const char*> getInstanceExtensions() const;
};

} // namespace Vulkan
