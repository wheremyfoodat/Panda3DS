#include "renderer_vk/renderer_vk.hpp"

#include <limits>
#include <span>
#include <unordered_set>

#include "SDL_vulkan.h"
#include "helpers.hpp"
#include "renderer_vk/vk_debug.hpp"
#include "renderer_vk/vk_memory.hpp"
#include "renderer_vk/vk_pica.hpp"

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

vk::RenderPass RendererVK::getRenderPass(PICA::ColorFmt colorFormat, std::optional<PICA::DepthFmt> depthFormat) {
	u32 renderPassHash = static_cast<u16>(colorFormat);

	if (depthFormat.has_value()) {
		renderPassHash |= (static_cast<u32>(depthFormat.value()) << 8);
	}

	// Cache hit
	if (renderPassCache.contains(renderPassHash)) {
		return renderPassCache.at(renderPassHash).get();
	}

	// Cache miss
	vk::RenderPassCreateInfo renderPassInfo = {};

	std::vector<vk::AttachmentDescription> renderPassAttachments = {};

	vk::AttachmentDescription colorAttachment = {};
	colorAttachment.format = Vulkan::colorFormatToVulkan(colorFormat);
	colorAttachment.samples = vk::SampleCountFlagBits::e1;
	colorAttachment.loadOp = vk::AttachmentLoadOp::eLoad;
	colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
	colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eLoad;
	colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eStore;
	colorAttachment.initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
	colorAttachment.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
	renderPassAttachments.emplace_back(colorAttachment);

	if (depthFormat.has_value()) {
		vk::AttachmentDescription depthAttachment = {};
		depthAttachment.format = Vulkan::depthFormatToVulkan(depthFormat.value());
		depthAttachment.samples = vk::SampleCountFlagBits::e1;
		depthAttachment.loadOp = vk::AttachmentLoadOp::eLoad;
		depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
		depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eLoad;
		depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eStore;
		depthAttachment.initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
		depthAttachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
		renderPassAttachments.emplace_back(depthAttachment);
	}

	renderPassInfo.setAttachments(renderPassAttachments);

	if (auto createResult = device->createRenderPassUnique(renderPassInfo); createResult.result == vk::Result::eSuccess) {
		return (renderPassCache[renderPassHash] = std::move(createResult.value)).get();
	} else {
		Helpers::panic("Error creating render pass: %s\n", vk::to_string(createResult.result).c_str());
	}
	return {};
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

	return vk::Result::eSuccess;
}

RendererVK::RendererVK(GPU& gpu, const std::array<u32, regNum>& internalRegs, const std::array<u32, extRegNum>& externalRegs)
	: Renderer(gpu, internalRegs, externalRegs) {}

RendererVK::~RendererVK() {}

void RendererVK::reset() {}

void RendererVK::display() {
	// Block, on the CPU, to ensure that this buffered-frame is ready for more work
	if (auto waitResult = device->waitForFences({frameFinishedFences[frameBufferingIndex].get()}, true, std::numeric_limits<u64>::max());
		waitResult != vk::Result::eSuccess) {
		Helpers::panic("Error waiting on swapchain fence: %s\n", vk::to_string(waitResult).c_str());
	}

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

	const vk::UniqueCommandBuffer& frameCommandBuffer = frameCommandBuffers[frameBufferingIndex];

	vk::CommandBufferBeginInfo beginInfo = {};
	beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;

	if (const vk::Result beginResult = frameCommandBuffer->begin(beginInfo); beginResult != vk::Result::eSuccess) {
		Helpers::panic("Error beginning command buffer recording: %s\n", vk::to_string(beginResult).c_str());
	}

	//// Render Frame(Simulated - just clear the images to different colors for now)
	{
		static const std::array<float, 4> frameScopeColor = {{1.0f, 0.0f, 1.0f, 1.0f}};

		Vulkan::DebugLabelScope debugScope(frameCommandBuffer.get(), frameScopeColor, "Frame");

		// Prepare images for color-clear
		frameCommandBuffer->pipelineBarrier(
			vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {},
			{
				vk::ImageMemoryBarrier(
					vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eTransferWrite, vk::ImageLayout::eUndefined,
					vk::ImageLayout::eTransferDstOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
					topScreenImages[frameBufferingIndex].get(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
				),
				vk::ImageMemoryBarrier(
					vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eTransferWrite, vk::ImageLayout::eUndefined,
					vk::ImageLayout::eTransferDstOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
					bottomScreenImages[frameBufferingIndex].get(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
				),
			}
		);
		static const std::array<float, 4> topClearColor = {{1.0f, 0.0f, 0.0f, 1.0f}};
		static const std::array<float, 4> bottomClearColor = {{0.0f, 1.0f, 0.0f, 1.0f}};
		frameCommandBuffer->clearColorImage(
			topScreenImages[frameBufferingIndex].get(), vk::ImageLayout::eTransferDstOptimal, topClearColor,
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
		);
		frameCommandBuffer->clearColorImage(
			bottomScreenImages[frameBufferingIndex].get(), vk::ImageLayout::eTransferDstOptimal, bottomClearColor,
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
		);
	}

	//// Present
	if (swapchainImageIndex != swapchainImageInvalid) {
		static const std::array<float, 4> presentScopeColor = {{1.0f, 1.0f, 0.0f, 1.0f}};
		Vulkan::DebugLabelScope debugScope(frameCommandBuffer.get(), presentScopeColor, "Present");

		// Prepare swapchain image for color-clear/blit-dst, prepare top/bottom screen for blit-src
		frameCommandBuffer->pipelineBarrier(
			vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {},
			{
				vk::ImageMemoryBarrier(
					vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eTransferWrite, vk::ImageLayout::eUndefined,
					vk::ImageLayout::eTransferDstOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, swapchainImages[swapchainImageIndex],
					vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
				),
				vk::ImageMemoryBarrier(
					vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead, vk::ImageLayout::eTransferDstOptimal,
					vk::ImageLayout::eTransferSrcOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
					topScreenImages[frameBufferingIndex].get(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
				),
				vk::ImageMemoryBarrier(
					vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead, vk::ImageLayout::eTransferDstOptimal,
					vk::ImageLayout::eTransferSrcOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
					bottomScreenImages[frameBufferingIndex].get(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
				),
			}
		);

		static const std::array<float, 4> clearColor = {{0.0f, 0.0f, 0.0f, 1.0f}};
		frameCommandBuffer->clearColorImage(
			swapchainImages[swapchainImageIndex], vk::ImageLayout::eTransferDstOptimal, clearColor,
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
		);

		// Blit top/bottom screen into swapchain image

		static const vk::ImageBlit topScreenBlit(
			vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), {vk::Offset3D{}, vk::Offset3D{400, 240, 1}},
			vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), {vk::Offset3D{}, vk::Offset3D{400, 240, 1}}
		);
		static const vk::ImageBlit bottomScreenBlit(
			vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), {vk::Offset3D{}, vk::Offset3D{320, 240, 1}},
			vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
			{vk::Offset3D{(400 / 2) - (320 / 2), 240, 0}, vk::Offset3D{(400 / 2) + (320 / 2), 240 + 240, 1}}
		);
		frameCommandBuffer->blitImage(
			topScreenImages[frameBufferingIndex].get(), vk::ImageLayout::eTransferSrcOptimal, swapchainImages[swapchainImageIndex],
			vk::ImageLayout::eTransferDstOptimal, {topScreenBlit}, vk::Filter::eNearest
		);
		frameCommandBuffer->blitImage(
			bottomScreenImages[frameBufferingIndex].get(), vk::ImageLayout::eTransferSrcOptimal, swapchainImages[swapchainImageIndex],
			vk::ImageLayout::eTransferDstOptimal, {bottomScreenBlit}, vk::Filter::eNearest
		);

		// Prepare swapchain image for present
		frameCommandBuffer->pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::DependencyFlags(), {}, {},
			{vk::ImageMemoryBarrier(
				vk::AccessFlagBits::eNone, vk::AccessFlagBits::eColorAttachmentWrite, vk::ImageLayout::eTransferDstOptimal,
				vk::ImageLayout::ePresentSrcKHR, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, swapchainImages[swapchainImageIndex],
				vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
			)}
		);
	}

	if (const vk::Result endResult = frameCommandBuffer->end(); endResult != vk::Result::eSuccess) {
		Helpers::panic("Error ending command buffer recording: %s\n", vk::to_string(endResult).c_str());
	}

	vk::SubmitInfo submitInfo = {};
	// Wait for any previous uses of the image image to finish presenting
	if (swapchainImageIndex != swapchainImageInvalid) {
		submitInfo.setWaitSemaphores(swapImageFreeSemaphore[frameBufferingIndex].get());
		static const vk::PipelineStageFlags waitStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		submitInfo.setWaitDstStageMask(waitStageMask);
	}
	// Signal when finished
	submitInfo.setSignalSemaphores(renderFinishedSemaphore[frameBufferingIndex].get());

	submitInfo.setCommandBuffers(frameCommandBuffer.get());

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

	frameBufferingIndex = ((frameBufferingIndex + 1) % frameBufferingCount);
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

	presentQueue = device->getQueue(presentQueueFamily, 0);
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

	// Frame-buffering synchronization primitives
	vk::FenceCreateInfo fenceInfo = {};
	fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

	vk::SemaphoreCreateInfo semaphoreInfo = {};

	swapImageFreeSemaphore.resize(frameBufferingCount);
	renderFinishedSemaphore.resize(frameBufferingCount);
	frameFinishedFences.resize(frameBufferingCount);

	vk::ImageCreateInfo topScreenInfo = {};
	topScreenInfo.setImageType(vk::ImageType::e2D);
	topScreenInfo.setFormat(vk::Format::eR8G8B8A8Unorm);
	topScreenInfo.setExtent(vk::Extent3D(400, 240, 1));
	topScreenInfo.setMipLevels(1);
	topScreenInfo.setArrayLayers(1);
	topScreenInfo.setSamples(vk::SampleCountFlagBits::e1);
	topScreenInfo.setTiling(vk::ImageTiling::eOptimal);
	topScreenInfo.setUsage(
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment | vk::ImageUsageFlagBits::eTransferSrc |
		vk::ImageUsageFlagBits::eTransferDst
	);
	topScreenInfo.setSharingMode(vk::SharingMode::eExclusive);
	topScreenInfo.setInitialLayout(vk::ImageLayout::eUndefined);

	vk::ImageCreateInfo bottomScreenInfo = topScreenInfo;
	bottomScreenInfo.setExtent(vk::Extent3D(320, 240, 1));

	topScreenImages.resize(frameBufferingCount);
	bottomScreenImages.resize(frameBufferingCount);

	for (usize i = 0; i < frameBufferingCount; i++) {
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

		if (auto createResult = device->createImageUnique(topScreenInfo); createResult.result == vk::Result::eSuccess) {
			topScreenImages[i] = std::move(createResult.value);
		} else {
			Helpers::panic("Error creating top-screen image: %s\n", vk::to_string(createResult.result).c_str());
		}

		if (auto createResult = device->createImageUnique(bottomScreenInfo); createResult.result == vk::Result::eSuccess) {
			bottomScreenImages[i] = std::move(createResult.value);
		} else {
			Helpers::panic("Error creating bottom-screen image: %s\n", vk::to_string(createResult.result).c_str());
		}
	}

	// Commit memory to all of our images
	{
		const auto getImage = [](const vk::UniqueImage& image) -> vk::Image { return image.get(); };
		std::vector<vk::Image> images;
		std::transform(topScreenImages.begin(), topScreenImages.end(), std::back_inserter(images), getImage);
		std::transform(bottomScreenImages.begin(), bottomScreenImages.end(), std::back_inserter(images), getImage);

		if (auto [result, imageMemory] = Vulkan::commitImageHeap(device.get(), physicalDevice, images); result == vk::Result::eSuccess) {
			framebufferMemory = std::move(imageMemory);
		} else {
			Helpers::panic("Error allocating framebuffer memory: %s\n", vk::to_string(result).c_str());
		}
	}
}

void RendererVK::clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) {}

void RendererVK::displayTransfer(u32 inputAddr, u32 outputAddr, u32 inputSize, u32 outputSize, u32 flags) {}

void RendererVK::textureCopy(u32 inputAddr, u32 outputAddr, u32 totalBytes, u32 inputSize, u32 outputSize, u32 flags) {}

void RendererVK::drawVertices(PICA::PrimType primType, std::span<const PICA::Vertex> vertices) {
	const u32 depthControl = regs[PICA::InternalRegs::DepthAndColorMask];
	const bool depthEnable = depthControl & 1;

	const vk::RenderPass curRenderPass = getRenderPass(colourBufferFormat, depthEnable ? std::make_optional(depthBufferFormat) : std::nullopt);
}

void RendererVK::screenshot(const std::string& name) {}
