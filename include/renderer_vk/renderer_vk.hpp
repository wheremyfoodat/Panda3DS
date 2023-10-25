#include <map>
#include <optional>

#include "math_util.hpp"
#include "renderer.hpp"
#include "vk_api.hpp"
#include "vk_descriptor_heap.hpp"
#include "vk_descriptor_update_batch.hpp"
#include "vk_sampler_cache.hpp"

class GPU;

class RendererVK final : public Renderer {
	SDL_Window* targetWindow;

	// The order of these `Unique*` members is important, they will be destroyed in RAII order
	vk::UniqueInstance instance = {};
	vk::UniqueDebugUtilsMessengerEXT debugMessenger = {};

	vk::SurfaceKHR swapchainSurface = {};

	vk::PhysicalDevice physicalDevice = {};

	vk::UniqueDevice device = {};

	vk::Queue presentQueue = {};
	u32 presentQueueFamily = ~0u;
	vk::Queue graphicsQueue = {};
	u32 graphicsQueueFamily = ~0u;
	vk::Queue computeQueue = {};
	u32 computeQueueFamily = ~0u;
	vk::Queue transferQueue = {};
	u32 transferQueueFamily = ~0u;

	vk::UniqueCommandPool commandPool = {};

	vk::UniqueSwapchainKHR swapchain = {};
	u32 swapchainImageCount = ~0u;
	std::vector<vk::Image> swapchainImages = {};
	std::vector<vk::UniqueImageView> swapchainImageViews = {};

	// This value is the degree of parallelism to allow multiple frames to be in-flight
	// aka: "double-buffer"/"triple-buffering"
	// Todo: make this a configuration option
	static constexpr usize frameBufferingCount = 3;

	// Frame-buffering data
	// Each vector is `frameBufferingCount` in size
	std::vector<vk::UniqueSemaphore> swapImageFreeSemaphore = {};
	std::vector<vk::UniqueSemaphore> renderFinishedSemaphore = {};
	std::vector<vk::UniqueFence> frameFinishedFences = {};
	std::vector<std::vector<vk::UniqueFramebuffer>> frameFramebuffers = {};
	std::vector<vk::UniqueCommandBuffer> frameCommandBuffers = {};

	const vk::CommandBuffer& getCurrentCommandBuffer() const { return frameCommandBuffers[frameBufferingIndex].get(); }

	// Todo:
	// Use `{colourBuffer,depthBuffer}Loc` to maintain an std::map-cache of framebuffers
	struct Texture {
		u32 loc = 0;
		u32 sizePerPixel = 0;
		std::array<u32, 2> size = {};

		vk::Format format;
		vk::UniqueImage image;
		vk::UniqueDeviceMemory imageMemory;
		vk::UniqueImageView imageView;

		Math::Rect<u32> getSubRect(u32 inputAddress, u32 width, u32 height) {
			// PICA textures have top-left origin, same as Vulkan
			const u32 startOffset = (inputAddress - loc) / sizePerPixel;
			const u32 x0 = (startOffset % (size[0] * 8)) / 8;
			const u32 y0 = (startOffset / (size[0] * 8)) * 8;
			return Math::Rect<u32>{x0, y0, x0 + width, y0 + height};
		}
	};
	// Hash(loc, size, format) -> Texture
	std::map<u64, Texture> textureCache;

	Texture* findRenderTexture(u32 addr);
	Texture& getColorRenderTexture(u32 addr, PICA::ColorFmt format, u32 width, u32 height);
	Texture& getDepthRenderTexture(u32 addr, PICA::DepthFmt format, u32 width, u32 height);

	// Framebuffer for the top/bottom image
	std::vector<vk::UniqueImage> screenTexture = {};
	std::vector<vk::UniqueImageView> screenTextureViews = {};
	std::vector<vk::UniqueFramebuffer> screenTextureFramebuffers = {};
	vk::UniqueDeviceMemory framebufferMemory = {};

	std::map<u64, vk::UniqueRenderPass> renderPassCache;

	vk::RenderPass getRenderPass(vk::Format colorFormat, std::optional<vk::Format> depthFormat);
	vk::RenderPass getRenderPass(PICA::ColorFmt colorFormat, std::optional<PICA::DepthFmt> depthFormat);

	std::unique_ptr<Vulkan::DescriptorUpdateBatch> descriptorUpdateBatch;
	std::unique_ptr<Vulkan::SamplerCache> samplerCache;

	// Display pipeline data
	std::unique_ptr<Vulkan::DescriptorHeap> displayDescriptorHeap;
	vk::UniquePipeline displayPipeline;
	vk::UniquePipelineLayout displayPipelineLayout;
	std::vector<vk::DescriptorSet> topDisplayPipelineDescriptorSet;
	std::vector<vk::DescriptorSet> bottomDisplayPipelineDescriptorSet;

	// Recreate the swapchain, possibly re-using the old one in the case of a resize
	vk::Result recreateSwapchain(vk::SurfaceKHR surface, vk::Extent2D swapchainExtent);

	u64 frameBufferingIndex = 0;

  public:
	RendererVK(GPU& gpu, const std::array<u32, regNum>& internalRegs, const std::array<u32, extRegNum>& externalRegs);
	~RendererVK() override;

	void reset() override;
	void display() override;
	void initGraphicsContext(SDL_Window* window) override;
	void clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) override;
	void displayTransfer(u32 inputAddr, u32 outputAddr, u32 inputSize, u32 outputSize, u32 flags) override;
	void textureCopy(u32 inputAddr, u32 outputAddr, u32 totalBytes, u32 inputSize, u32 outputSize, u32 flags) override;
	void drawVertices(PICA::PrimType primType, std::span<const PICA::Vertex> vertices) override;
	void screenshot(const std::string& name) override;
	void deinitGraphicsContext() override;
};
