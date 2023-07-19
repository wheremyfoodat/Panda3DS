#include "renderer.hpp"
#include "vulkan_api.hpp"

class GPU;

class RendererVK final : public Renderer {
	vk::UniqueInstance instance = {};
	vk::PhysicalDevice physicalDevice = {};
	vk::UniqueDevice device = {};

	vk::UniqueDebugUtilsMessengerEXT debugMessenger;

  public:
	RendererVK(GPU& gpu, const std::array<u32, regNum>& internalRegs);
	~RendererVK() override;

	void reset() override;
	void display() override;
	void initGraphicsContext() override;
	void clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) override;
	void displayTransfer(u32 inputAddr, u32 outputAddr, u32 inputSize, u32 outputSize, u32 flags) override;
	void drawVertices(PICA::PrimType primType, std::span<const PICA::Vertex> vertices) override;
	void screenshot(const std::string& name) override;
};