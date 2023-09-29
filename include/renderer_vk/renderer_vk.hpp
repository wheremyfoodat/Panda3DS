#include <map>
#include <optional>

#include "PICA/pica_uniforms.hpp"
#include "renderer.hpp"
#include "renderer_gl/surface_cache.hpp"
#include "vk_descriptor_heap.hpp"
#include "vk_descriptor_update_batch.hpp"
#include "vk_sampler_cache.hpp"
#include "vk_stream_buffer.hpp"
#include "vk_instance.hpp"
#include "vk_swapchain.hpp"
#include "vk_scheduler.hpp"
#include "vk_pipeline.hpp"
#include "vk_pipeline_manager.hpp"
#include "vk_render_manager.hpp"
#include "vk_surfaces.hpp"

class GPU;
class Window;

class RendererVK final : public Renderer {
    const Window& window;
    Vulkan::Instance instance;
    Vulkan::Scheduler scheduler;
    Vulkan::Swapchain swapchain;
    Vulkan::RenderManager renderManager;
    Vulkan::SamplerCache samplerCache;

	// Streaming buffers
	static constexpr u32 UniformSize = sizeof(VsSupportBuffer) + sizeof(FsSupportBuffer);
	static constexpr u32 UniformBufferSize = 256 * UniformSize;
	static constexpr u32 VertexSize = sizeof(PICA::Vertex);
	Vulkan::StreamBuffer<VertexSize> vertexBuffer;
	Vulkan::StreamBuffer<UniformSize> uniformBuffer;
	Vulkan::StreamBuffer<4096> uploadBuffer;
    Vulkan::PipelineManager pipelineManager;

    SurfaceCache<Vulkan::DepthBuffer, 16, true> depthBufferCache;
    SurfaceCache<Vulkan::ColourBuffer, 16, true> colourBufferCache;
    SurfaceCache<Vulkan::Texture, 256, true> textureCache;

    Vulkan::Texture lightingLut;
    Vulkan::Texture defaultTex;
    vk::Sampler sampler;

	// Display pipeline data
	vk::UniquePipelineLayout displayPipelineLayout;
	Vulkan::DescriptorUpdateBatch descriptorUpdateBatch;
	std::unique_ptr<Vulkan::DescriptorHeap> displayDescriptorHeap;
    std::unique_ptr<Vulkan::Pipeline> displayPipeline;
    std::vector<vk::DescriptorSet> displayPipelineDescriptorSets;
    Vulkan::PipelineInfo picaPipelineInfo{};

    bool prepareDisplay();
    void drawScreen(u32 screenIndex, vk::Rect2D drawArea);

  public:
    RendererVK(GPU& gpu, const Window& window, const std::array<u32, regNum>& internalRegs, const std::array<u32, extRegNum>& externalRegs);
	~RendererVK() override;

    Vulkan::Instance& getInstance() noexcept {
        return instance;
    }

    Vulkan::Scheduler& getScheduler() noexcept {
        return scheduler;
    }

	void setupStencilTest();
	void setupBlending();
	void setupTopologyAndCulling(PICA::PrimType primType);
	void bindTexturesToSlots();
	void uploadUniforms();
	void updateLightingLUT();

	vk::ImageView getTexture(Vulkan::Texture& tex);

	void reset() override;
	void display() override;
	void deinitGraphicsContext() override {}
	void clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) override;
	void displayTransfer(u32 inputAddr, u32 outputAddr, u32 inputSize, u32 outputSize, u32 flags) override;
	void textureCopy(u32 inputAddr, u32 outputAddr, u32 totalBytes, u32 inputSize, u32 outputSize, u32 flags) override;
	void drawVertices(PICA::PrimType primType, std::span<const PICA::Vertex> vertices) override;
	void screenshot(const std::string& name) override;

    std::optional<Vulkan::ColourBuffer> getColourBuffer(u32 addr, PICA::ColorFmt format, u32 width, u32 height, bool createIfnotFound = true);
    std::optional<Vulkan::DepthBuffer> getDepthBuffer();
};
