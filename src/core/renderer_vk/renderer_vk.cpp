#include "renderer_vk/renderer_vk.hpp"

#include "SDL_vulkan.h"
#include "helpers.hpp"
#include "renderer_vk/vk_debug.hpp"

RendererVK::RendererVK(GPU& gpu, const std::array<u32, regNum>& internalRegs) : Renderer(gpu, internalRegs) {}

RendererVK::~RendererVK() {}

void RendererVK::reset() {}

void RendererVK::display() {}

void RendererVK::initGraphicsContext(SDL_Window* window) {
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

	static const std::array instanceExtensions = std::to_array({
#if defined(__APPLE__)
		VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
#endif
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
	});

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
	if (std::find(
			instanceExtensions.begin(), instanceExtensions.end(),
			// std::string_view has a way to compare itself to `const char*`
			// so by casting it, we get the actual string comparisons
			// and not pointer-comparisons
			std::string_view(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)
		) != instanceExtensions.end()) {
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

	// Pick physical device
	if (auto EnumerateResult = instance->enumeratePhysicalDevices(); EnumerateResult.result == vk::Result::eSuccess) {
		std::vector<vk::PhysicalDevice> PhysicalDevices = std::move(EnumerateResult.value);

		// Prefer Discrete GPUs
		const auto IsDiscrete = [](const vk::PhysicalDevice& PhysicalDevice) -> bool {
			return PhysicalDevice.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
		};

		std::partition(PhysicalDevices.begin(), PhysicalDevices.end(), IsDiscrete);

		// Pick the "best" out of all of the previous criteria
		physicalDevice = PhysicalDevices.front();
	} else {
		Helpers::panic("Error enumerating physical devices: %s\n", vk::to_string(EnumerateResult.result).c_str());
	}

	// Create Device
	vk::DeviceCreateInfo deviceInfo = {};

	static const char* deviceExtensions[] = {
#if defined(__APPLE__)
		"VK_KHR_portability_subset",
#endif
		VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME
	};
	deviceInfo.ppEnabledExtensionNames = deviceExtensions;
	deviceInfo.enabledExtensionCount = std::size(deviceExtensions);

	vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceTimelineSemaphoreFeatures> deviceFeatureChain = {};

	auto& deviceFeatures = deviceFeatureChain.get<vk::PhysicalDeviceFeatures2>().features;

	auto& deviceTimelineFeatures = deviceFeatureChain.get<vk::PhysicalDeviceTimelineSemaphoreFeatures>();
	deviceTimelineFeatures.timelineSemaphore = true;

	deviceInfo.pNext = &deviceFeatureChain.get();

	static const float queuePriority = 1.0f;

	vk::DeviceQueueCreateInfo queueInfo = {};
	queueInfo.queueFamilyIndex = 0;
	queueInfo.queueCount = 1;
	queueInfo.pQueuePriorities = &queuePriority;

	deviceInfo.queueCreateInfoCount = 1;
	deviceInfo.pQueueCreateInfos = &queueInfo;

	if (auto createResult = physicalDevice.createDeviceUnique(deviceInfo); createResult.result == vk::Result::eSuccess) {
		device = std::move(createResult.value);
	} else {
		Helpers::panic("Error creating logical device: %s\n", vk::to_string(createResult.result).c_str());
	}

	// Initialize device-specific function pointers
	VULKAN_HPP_DEFAULT_DISPATCHER.init(device.get());

	VkSurfaceKHR surface;
	SDL_Vulkan_CreateSurface(window, instance.get(), &surface);
}

void RendererVK::clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) {}

void RendererVK::displayTransfer(u32 inputAddr, u32 outputAddr, u32 inputSize, u32 outputSize, u32 flags) {}

void RendererVK::drawVertices(PICA::PrimType primType, std::span<const PICA::Vertex> vertices) {}

void RendererVK::screenshot(const std::string& name) {}