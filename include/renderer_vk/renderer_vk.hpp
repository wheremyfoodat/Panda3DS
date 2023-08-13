#include <map>
#include <optional>

#include "renderer.hpp"
#include "vk_api.hpp"

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

	vk::UniqueDeviceMemory framebufferMemory = {};

	// Frame-buffering data
	// Each vector is `frameBufferingCount` in size
	std::vector<vk::UniqueCommandBuffer> framePresentCommandBuffers = {};
	std::vector<vk::UniqueSemaphore> swapImageFreeSemaphore = {};
	std::vector<vk::UniqueSemaphore> renderFinishedSemaphore = {};
	std::vector<vk::UniqueFence> frameFinishedFences = {};
	std::vector<std::vector<vk::UniqueFramebuffer>> frameFramebuffers = {};
	std::vector<vk::UniqueCommandBuffer> frameGraphicsCommandBuffers = {};

	// Todo:
	// Use `{colourBuffer,depthBuffer}Loc` to maintain an std::map-cache of framebuffers
	struct Texture {
		vk::UniqueImage image;
		vk::UniqueDeviceMemory imageMemory;
		vk::UniqueImageView imageView;
	};
	// Hash(loc, size, format) -> Texture
	std::map<u32, Texture> textureCache;

	static u32 colorBufferHash(u32 loc, u32 size, PICA::ColorFmt format);
	static u32 depthBufferHash(u32 loc, u32 size, PICA::DepthFmt format);

	Texture& getColorRenderTexture();
	Texture& getDepthRenderTexture();

	// Use `lower_bound` to find nearest texture for an address
	// Blit them during `display()`
	std::vector<vk::UniqueImage> screenTexture = {};

	std::map<u32, vk::UniqueRenderPass> renderPassCache;

	vk::RenderPass getRenderPass(PICA::ColorFmt colorFormat, std::optional<PICA::DepthFmt> depthFormat);

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
};
