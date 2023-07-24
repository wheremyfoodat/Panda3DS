#include "renderer.hpp"
#include "vulkan_api.hpp"

class GPU;

class RendererVK final : public Renderer {
	SDL_Window* targetWindow;

	// The order of these `Unique*` members is important, they will be destroyed in RAII order
	vk::UniqueInstance instance = {};
	vk::UniqueDebugUtilsMessengerEXT debugMessenger = {};

	vk::UniqueSurfaceKHR surface = {};

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

	// Per-swapchain-image data
	// Each vector is `swapchainImageCount` in size
	std::vector<vk::UniqueCommandBuffer> presentCommandBuffers = {};
	std::vector<vk::UniqueSemaphore> swapImageFreeSemaphore = {};
	std::vector<vk::UniqueSemaphore> renderFinishedSemaphore = {};
	std::vector<vk::UniqueFence> frameFinishedFences = {};

	// Recreate the swapchain, possibly re-using the old one in the case of a resize
	vk::Result recreateSwapchain(vk::SurfaceKHR surface, vk::Extent2D swapchainExtent);

	u64 currentFrame = 0;
  public:
	RendererVK(GPU& gpu, const std::array<u32, regNum>& internalRegs);
	~RendererVK() override;

	void reset() override;
	void display() override;
	void initGraphicsContext(SDL_Window* window) override;
	void clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) override;
	void displayTransfer(u32 inputAddr, u32 outputAddr, u32 inputSize, u32 outputSize, u32 flags) override;
	void drawVertices(PICA::PrimType primType, std::span<const PICA::Vertex> vertices) override;
	void screenshot(const std::string& name) override;
};