#include "helpers.hpp"
#include "renderer_vk/vk_instance.hpp"
#include "renderer_vk/vk_debug.hpp"
#include "SDL_vulkan.h"

#include <algorithm>
#include <vk_mem_alloc.h>

namespace Vulkan {

Instance::Instance(SDL_Window* window_, u32 physicalDevice, bool enableDebug_) : window(window_), enableDebug(enableDebug_) {
    createInstance();
    pickPhysicalDevice(physicalDevice);
    pickGraphicsQueue();
    createDevice();
    createAllocator();
}

Instance::~Instance() = default;

std::vector<const char*> Instance::getInstanceExtensions() const {
    u32 numExtensions = 1;
    std::vector<const char*> extensions;

    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

#if defined(__APPLE__)
    extensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    numExtensions++;
#endif

    if (enableDebug) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        numExtensions++;
    }

    // Get any additional extensions that SDL wants as well
    if (window) {
        u32 sdlExtensionCount = 0;
        SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionCount, nullptr);
        extensions.resize(numExtensions + sdlExtensionCount);
        SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionCount, &extensions[numExtensions]);
    }

    return extensions;
}

void Instance::createInstance() {
    const auto getInstaceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(getInstaceProcAddr);

    const vk::ApplicationInfo applicationInfo = {
        .pApplicationName = "Alber",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Alber",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };

    const auto instanceExtensions = getInstanceExtensions();
    vk::InstanceCreateInfo instanceInfo = {
        .pApplicationInfo = &applicationInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<u32>(instanceExtensions.size()),
        .ppEnabledExtensionNames = instanceExtensions.data(),
    };

#if defined(__APPLE__)
    instanceInfo.flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
#endif

    if (auto createResult = vk::createInstanceUnique(instanceInfo); createResult.result == vk::Result::eSuccess) {
        instance = std::move(createResult.value);
    } else {
        Helpers::panic("Error creating Vulkan instance: %s\n", vk::to_string(createResult.result).c_str());
    }

    // Initialize instance-specific function pointers
    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance.get());

    // Enable debug messenger if the instance was able to be created with debug_utils
    if (!enableDebug) {
        return;
    }

    const vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo = {
        .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                           vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
                           vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                           vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning,
        .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                       vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                       vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral,
        .pfnUserCallback = &Vulkan::debugMessageCallback,
    };

    if (auto createResult = instance->createDebugUtilsMessengerEXTUnique(debugCreateInfo); createResult.result == vk::Result::eSuccess) {
        debugMessenger = std::move(createResult.value);
    } else {
        Helpers::warn("Error registering debug messenger: %s", vk::to_string(createResult.result).c_str());
    }
}

void Instance::pickPhysicalDevice(u32 index) {
    const auto enumerateResult = instance->enumeratePhysicalDevices();
    if (enumerateResult.result != vk::Result::eSuccess) [[unlikely]] {
        Helpers::panic("Error enumerating physical devices: %s\n", vk::to_string(enumerateResult.result).c_str());
        return;
    }

    const auto physicalDevices = std::move(enumerateResult.value);
    if (index >= physicalDevices.size()) [[unlikely]] {
        Helpers::panic("Physical device index %d exceeds number of queried devices %lld\n",
                       index, physicalDevices.size());
        return;
    }
    physicalDevice = physicalDevices[index];
}

void Instance::pickGraphicsQueue() {
    const auto familyProperties = physicalDevice.getQueueFamilyProperties();
    if (familyProperties.empty()) [[unlikely]] {
        Helpers::panic("Physical device reported no queues.");
        return;
    }

    // Find a queue family that supports both graphics and transfer for our use.
    // We can assume that this queue will also support present as this is the case
    // on all modern hardware.
    for (size_t i = 0; i < familyProperties.size(); i++) {
        const u32 index = static_cast<u32>(i);
        const auto flags = familyProperties[i].queueFlags;
        if (flags & vk::QueueFlagBits::eGraphics && flags & vk::QueueFlagBits::eTransfer) {
            queueFamilyIndex = index;
            break;
        }
    }

    if (queueFamilyIndex == -1) {
        Helpers::panic("Unable to find suitable graphics queue\n");
    }
}

void Instance::createDevice() {
    // Query for a few useful extensions for 3DS emulation
    const vk::StructureChain featureChain = physicalDevice.getFeatures2<
        vk::PhysicalDeviceFeatures2,
#if defined(__APPLE__)
        vk::PhysicalDevicePortabilitySubsetFeaturesKHR,
#endif
        vk::PhysicalDeviceTimelineSemaphoreFeaturesKHR,
        vk::PhysicalDeviceCustomBorderColorFeaturesEXT,
        vk::PhysicalDeviceIndexTypeUint8FeaturesEXT>();

    features = featureChain.get().features;

    const auto [result, extensions] = physicalDevice.enumerateDeviceExtensionProperties();
    std::array<const char*, 10> enabledExtensions;

    static constexpr float queuePriority = 1.0f;

    const vk::DeviceQueueCreateInfo queueInfo = {
        .queueFamilyIndex = static_cast<u32>(queueFamilyIndex),
        .queueCount = 1u,
        .pQueuePriorities = &queuePriority,
    };

    vk::StructureChain deviceChain = {
        vk::DeviceCreateInfo{
            .queueCreateInfoCount = 1u,
            .pQueueCreateInfos = &queueInfo,
            .enabledExtensionCount = 0,
            .ppEnabledExtensionNames = enabledExtensions.data(),
        },
        vk::PhysicalDeviceFeatures2{
            .features{
                .robustBufferAccess = features.robustBufferAccess,
                .logicOp = features.logicOp,
                .samplerAnisotropy = features.samplerAnisotropy,
                .shaderClipDistance = features.shaderClipDistance,
            },
        },
#if defined(__APPLE__)
        vk::PhysicalDevicePortabilitySubsetFeaturesKHR{},
#endif
        vk::PhysicalDeviceTimelineSemaphoreFeaturesKHR{
            .timelineSemaphore = true,
        },
        vk::PhysicalDeviceCustomBorderColorFeaturesEXT{
            .customBorderColorWithoutFormat = true,
        },
        vk::PhysicalDeviceIndexTypeUint8FeaturesEXT{
            .indexTypeUint8 = true,
        },
    };

    const auto addExtension = [&](std::string_view extension) -> bool {
        const auto result =
            std::find_if(extensions.begin(), extensions.end(),
                         [&](const auto& prop) { return extension.compare(prop.extensionName.data()) == 0; });

        if (result != extensions.end()) {
            printf("Using vulkan extension %s\n", extension.data());
            u32& numExtensions = deviceChain.get().enabledExtensionCount;
            enabledExtensions[numExtensions++] = extension.data();
            return true;
        }

        printf("Requested extension %s is unavailable\n", extension.data());
        return false;
    };

    // Helper macro that checks for an extension and any required features
#define USE_EXTENSION(name, feature_type, feature, property) \
    property = addExtension(name) && featureChain.get<feature_type>().feature;  \
    if (!property) { \
        deviceChain.unlink<feature_type>(); \
    }

    addExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
#if defined(__APPLE__)
    portabilitySubset = useExtension(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif
    USE_EXTENSION(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME, vk::PhysicalDeviceTimelineSemaphoreFeaturesKHR,
                  timelineSemaphore, timelineSemaphores);
    USE_EXTENSION(VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME, vk::PhysicalDeviceCustomBorderColorFeaturesEXT,
                  customBorderColorWithoutFormat, customBorderColor);
    USE_EXTENSION(VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME, vk::PhysicalDeviceIndexTypeUint8FeaturesEXT,
                  indexTypeUint8, indexTypeUint8);
#undef USE_EXTENSION

    if (auto createResult = physicalDevice.createDeviceUnique(deviceChain.get()); createResult.result == vk::Result::eSuccess) {
        device = std::move(createResult.value);
    } else {
        Helpers::panic("Error creating logical device: %s\n", vk::to_string(createResult.result).c_str());
    }

    // Initialize device-specific function pointers
    VULKAN_HPP_DEFAULT_DISPATCHER.init(device.get());
    graphicsQueue = device->getQueue(queueFamilyIndex, 0);
}

void Instance::createAllocator() {
    const VmaVulkanFunctions functions = {
        .vkGetInstanceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr,
        .vkGetDeviceBufferMemoryRequirements = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceBufferMemoryRequirements,
        .vkGetDeviceImageMemoryRequirements = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceImageMemoryRequirements,
    };

    const VmaAllocatorCreateInfo allocatorInfo = {
        .physicalDevice = physicalDevice,
        .device = *device,
        .pVulkanFunctions = &functions,
        .instance = *instance,
        .vulkanApiVersion = VK_MAKE_VERSION(1, 2, 0),
    };

    const VkResult result = vmaCreateAllocator(&allocatorInfo, &allocator);
    if (result != VK_SUCCESS) {
        Helpers::panic("Failed to initialize VMA with error %d\n", result);
    }
}

} // namespace Vulkan
