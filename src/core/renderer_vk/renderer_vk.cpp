#include "renderer_vk/renderer_vk.hpp"

#include <limits>
#include <span>
#include <unordered_set>

#include "SDL_vulkan.h"
#include "helpers.hpp"
#include "renderer_vk/vk_debug.hpp"

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
		std::vector<vk::PresentModeKHR>& presentModes = getResult.value;

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

	// Swapchain Command buffer(s)
	vk::CommandBufferAllocateInfo commandBuffersInfo = {};
	commandBuffersInfo.commandPool = commandPool.get();
	commandBuffersInfo.level = vk::CommandBufferLevel::ePrimary;
	commandBuffersInfo.commandBufferCount = swapchainImageCount;

	if (auto allocateResult = device->allocateCommandBuffersUnique(commandBuffersInfo); allocateResult.result == vk::Result::eSuccess) {
		presentCommandBuffers = std::move(allocateResult.value);
	} else {
		Helpers::panic("Error allocating command buffer: %s\n", vk::to_string(allocateResult.result).c_str());
	}

	// Swapchain synchronization primitives
	vk::FenceCreateInfo fenceInfo = {};
	fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

	vk::SemaphoreCreateInfo semaphoreInfo = {};

	swapImageFreeSemaphore.resize(swapchainImageCount);
	renderFinishedSemaphore.resize(swapchainImageCount);
	frameFinishedFences.resize(swapchainImageCount);

	for (usize i = 0; i < swapchainImageCount; i++) {
		if (auto createResult = device->createSemaphoreUnique(semaphoreInfo); createResult.result == vk::Result::eSuccess) {
			swapImageFreeSemaphore[i] = std::move(createResult.value);
		} else {
			Helpers::panic("Error creating 'present-ready' semaphore: %s\n", vk::to_string(createResult.result).c_str());
		}

		if (auto createResult = device->createSemaphoreUnique(semaphoreInfo); createResult.result == vk::Result::eSuccess) {
			renderFinishedSemaphore[i] = std::move(createResult.value);
		} else {
			Helpers::panic("Error creating 'post-render' semaphore: %s\n", vk::to_string(createResult.result).c_str());
		}

		if (auto createResult = device->createFenceUnique(fenceInfo); createResult.result == vk::Result::eSuccess) {
			frameFinishedFences[i] = std::move(createResult.value);
		} else {
			Helpers::panic("Error creating 'present-ready' semaphore: %s\n", vk::to_string(createResult.result).c_str());
		}
	}

	return vk::Result::eSuccess;
}

RendererVK::RendererVK(GPU& gpu, const std::array<u32, regNum>& internalRegs) : Renderer(gpu, internalRegs) {}

RendererVK::~RendererVK() {}

void RendererVK::reset() {}

void RendererVK::display() {
	// Block, on the CPU, to ensure that this swapchain-frame is ready for more work
	if (auto waitResult = device->waitForFences({frameFinishedFences[currentFrame].get()}, true, std::numeric_limits<u64>::max());
		waitResult != vk::Result::eSuccess) {
		Helpers::panic("Error waiting on swapchain fence: %s\n", vk::to_string(waitResult).c_str());
	}

	u32 swapchainImageIndex = std::numeric_limits<u32>::max();
	if (const auto acquireResult =
			device->acquireNextImageKHR(swapchain.get(), std::numeric_limits<u64>::max(), swapImageFreeSemaphore[currentFrame].get(), {});
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
				recreateSwapchain(surface.get(), swapchainExtent);
				break;
			}
			default: {
				Helpers::panic("Error acquiring next swapchain image: %s\n", vk::to_string(acquireResult.result).c_str());
			}
		}
	}

	vk::UniqueCommandBuffer& presentCommandBuffer = presentCommandBuffers.at(currentFrame);

	vk::CommandBufferBeginInfo beginInfo = {};
	beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;

	if (const vk::Result beginResult = presentCommandBuffer->begin(beginInfo); beginResult != vk::Result::eSuccess) {
		Helpers::panic("Error beginning command buffer recording: %s\n", vk::to_string(beginResult).c_str());
	}

	{
		static const std::array<float, 4> presentScopeColor = {{1.0f, 0.0f, 1.0f, 1.0f}};

		Vulkan::DebugLabelScope debugScope(presentCommandBuffer.get(), presentScopeColor, "Present");

		// Prepare for color-clear
		presentCommandBuffer->pipelineBarrier(
			vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {},
			{vk::ImageMemoryBarrier(
				vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eTransferWrite, vk::ImageLayout::eUndefined,
				vk::ImageLayout::eTransferDstOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, swapchainImages[swapchainImageIndex],
				vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
			)}
		);

		presentCommandBuffer->clearColorImage(
			swapchainImages[swapchainImageIndex], vk::ImageLayout::eTransferDstOptimal, presentScopeColor,
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
		);

		// Prepare for present
		presentCommandBuffer->pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::DependencyFlags(), {}, {},
			{vk::ImageMemoryBarrier(
				vk::AccessFlagBits::eNone, vk::AccessFlagBits::eColorAttachmentWrite, vk::ImageLayout::eTransferDstOptimal,
				vk::ImageLayout::ePresentSrcKHR, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, swapchainImages[swapchainImageIndex],
				vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
			)}
		);
	}

	if (const vk::Result endResult = presentCommandBuffer->end(); endResult != vk::Result::eSuccess) {
		Helpers::panic("Error ending command buffer recording: %s\n", vk::to_string(endResult).c_str());
	}

	vk::SubmitInfo submitInfo = {};
	// Wait for any previous uses of the image image to finish presenting
	submitInfo.setWaitSemaphores(swapImageFreeSemaphore[currentFrame].get());
	// Signal when finished
	submitInfo.setSignalSemaphores(renderFinishedSemaphore[currentFrame].get());

	static const vk::PipelineStageFlags waitStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	submitInfo.setWaitDstStageMask(waitStageMask);

	submitInfo.setCommandBuffers(presentCommandBuffer.get());

	device->resetFences({frameFinishedFences[currentFrame].get()});

	if (const vk::Result submitResult = graphicsQueue.submit({submitInfo}, frameFinishedFences[currentFrame].get());
		submitResult != vk::Result::eSuccess) {
		Helpers::panic("Error submitting to graphics queue: %s\n", vk::to_string(submitResult).c_str());
	}

	vk::PresentInfoKHR presentInfo = {};
	presentInfo.setWaitSemaphores(renderFinishedSemaphore[currentFrame].get());
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
				recreateSwapchain(surface.get(), swapchainExtent);
				break;
			}
			default: {
				Helpers::panic("Error presenting swapchain image: %s\n", vk::to_string(presentResult).c_str());
			}
		}
	}

	currentFrame = ((currentFrame + 1) % swapchainImageCount);
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

	std::vector<const char*> instanceExtensions = {
#if defined(__APPLE__)
		VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
#endif
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
	};

	// Get any additional extensions that SDL wants as well
	{
		unsigned int extensionCount = 0;
		SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr);
		std::vector<const char*> sdlInstanceExtensions(extensionCount);
		SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, sdlInstanceExtensions.data());

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

	// Create surface
	if (VkSurfaceKHR newSurface; SDL_Vulkan_CreateSurface(window, instance.get(), &newSurface)) {
		surface.reset(newSurface);
	} else {
		Helpers::warn("Error creating Vulkan surface");
	}

	// Pick physical device
	if (auto enumerateResult = instance->enumeratePhysicalDevices(); enumerateResult.result == vk::Result::eSuccess) {
		std::vector<vk::PhysicalDevice> physicalDevices = std::move(enumerateResult.value);
		std::vector<vk::PhysicalDevice>::iterator partitionEnd = physicalDevices.end();

		// Prefer GPUs that can access the surface
		const auto surfaceSupport = [this](const vk::PhysicalDevice& physicalDevice) -> bool {
			const usize queueCount = physicalDevice.getQueueFamilyProperties().size();
			for (usize queueIndex = 0; queueIndex < queueCount; ++queueIndex) {
				if (auto supportResult = physicalDevice.getSurfaceSupportKHR(queueIndex, surface.get());
					supportResult.result == vk::Result::eSuccess) {
					return supportResult.value;
				}
			}
			return false;
		};

		partitionEnd = std::stable_partition(physicalDevices.begin(), partitionEnd, surfaceSupport);

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

		// Get present queue family
		for (usize queueFamilyIndex = 0; queueFamilyIndex < queueFamilyProperties.size(); ++queueFamilyIndex) {
			if (auto supportResult = physicalDevice.getSurfaceSupportKHR(queueFamilyIndex, surface.get());
				supportResult.result == vk::Result::eSuccess) {
				if (supportResult.value) {
					presentQueueFamily = queueFamilyIndex;
					break;
				}
			}
		}

		static const float queuePriority = 1.0f;

		graphicsQueueFamily = findQueueFamily(queueFamilyProperties, vk::QueueFlagBits::eGraphics);
		computeQueueFamily = findQueueFamily(queueFamilyProperties, vk::QueueFlagBits::eCompute);
		transferQueueFamily = findQueueFamily(queueFamilyProperties, vk::QueueFlagBits::eTransfer);

		// Requests a singular queue for each unique queue-family
		const std::unordered_set<u32> queueFamilyRequests = {presentQueueFamily, graphicsQueueFamily, computeQueueFamily, transferQueueFamily};
		for (const u32 queueFamilyIndex : queueFamilyRequests) {
			deviceQueueInfos.emplace_back(vk::DeviceQueueCreateInfo({}, queueFamilyIndex, 1, &queuePriority));
		}
	}

	// Create Device
	vk::DeviceCreateInfo deviceInfo = {};

	static const char* deviceExtensions[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#if defined(__APPLE__)
		"VK_KHR_portability_subset",
#endif
		// VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME
	};
	deviceInfo.ppEnabledExtensionNames = deviceExtensions;
	deviceInfo.enabledExtensionCount = std::size(deviceExtensions);

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

	presentQueue = device->getQueue(presentQueueFamily, 0);
	graphicsQueue = device->getQueue(presentQueueFamily, 0);
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
	vk::Extent2D swapchainExtent;
	{
		int windowWidth, windowHeight;
		SDL_Vulkan_GetDrawableSize(window, &windowWidth, &windowHeight);
		swapchainExtent.width = windowWidth;
		swapchainExtent.height = windowHeight;
	}
	recreateSwapchain(surface.get(), swapchainExtent);
}

void RendererVK::clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) {}

void RendererVK::displayTransfer(u32 inputAddr, u32 outputAddr, u32 inputSize, u32 outputSize, u32 flags) {}

void RendererVK::drawVertices(PICA::PrimType primType, std::span<const PICA::Vertex> vertices) {}

void RendererVK::screenshot(const std::string& name) {}