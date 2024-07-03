#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include "renderer.hpp"
#include "texture.hpp"
#include "render_target.hpp"
#include "mtl_pipeline_cache.hpp"
#include "mtl_depth_stencil_cache.hpp"
#include "mtl_vertex_buffer_cache.hpp"
// HACK: use the OpenGL cache
#include "../renderer_gl/surface_cache.hpp"

class GPU;

class RendererMTL final : public Renderer {
  public:
	RendererMTL(GPU& gpu, const std::array<u32, regNum>& internalRegs, const std::array<u32, extRegNum>& externalRegs);
	~RendererMTL() override;

	void reset() override;
	void display() override;
	void initGraphicsContext(SDL_Window* window) override;
	void clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) override;
	void displayTransfer(u32 inputAddr, u32 outputAddr, u32 inputSize, u32 outputSize, u32 flags) override;
	void textureCopy(u32 inputAddr, u32 outputAddr, u32 totalBytes, u32 inputSize, u32 outputSize, u32 flags) override;
	void drawVertices(PICA::PrimType primType, std::span<const PICA::Vertex> vertices) override;
	void screenshot(const std::string& name) override;
	void deinitGraphicsContext() override;

#ifdef PANDA3DS_FRONTEND_QT
	virtual void initGraphicsContext([[maybe_unused]] GL::Context* context) override {}
#endif

  private:
	CA::MetalLayer* metalLayer;

	MTL::Device* device;
	MTL::CommandQueue* commandQueue;

	// Caches
	SurfaceCache<Metal::ColorRenderTarget, 16, true> colorRenderTargetCache;
	SurfaceCache<Metal::DepthStencilRenderTarget, 16, true> depthStencilRenderTargetCache;
	SurfaceCache<Metal::Texture, 256, true> textureCache;
	Metal::PipelineCache blitPipelineCache;
	Metal::PipelineCache drawPipelineCache;
	Metal::DepthStencilCache depthStencilCache;
	Metal::VertexBufferCache vertexBufferCache;

	// Helpers
	MTL::SamplerState* basicSampler;

	// Pipelines
	MTL::RenderPipelineState* displayPipeline;

	// Active state
	MTL::CommandBuffer* commandBuffer = nullptr;
	MTL::RenderCommandEncoder* renderCommandEncoder = nullptr;
	MTL::Texture* lastColorTexture = nullptr;
	MTL::Texture* lastDepthTexture = nullptr;

	void createCommandBufferIfNeeded() {
		if (!commandBuffer) {
			commandBuffer = commandQueue->commandBuffer();
		}
	}

	void endRenderPass() {
        if (renderCommandEncoder) {
            renderCommandEncoder->endEncoding();
            renderCommandEncoder = nullptr;
        }
	}

	void beginRenderPassIfNeeded(MTL::RenderPassDescriptor* renderPassDescriptor, MTL::Texture* colorTexture, MTL::Texture* depthTexture) {
		createCommandBufferIfNeeded();

		if (!renderCommandEncoder || colorTexture != lastColorTexture || depthTexture != lastDepthTexture) {
		    endRenderPass();

            renderCommandEncoder = commandBuffer->renderCommandEncoder(renderPassDescriptor);

		    lastColorTexture = colorTexture;
            lastDepthTexture = depthTexture;
		}
	}

	void commitCommandBuffer() {
	   if (renderCommandEncoder) {
            renderCommandEncoder->endEncoding();
            renderCommandEncoder = nullptr;
        }
        if (commandBuffer) {
            commandBuffer->commit();
            commandBuffer = nullptr;
        }
    }

	std::optional<Metal::ColorRenderTarget> getColorRenderTarget(u32 addr, PICA::ColorFmt format, u32 width, u32 height, bool createIfnotFound = true);
	Metal::DepthStencilRenderTarget& getDepthRenderTarget();
	MTL::Texture* getTexture(Metal::Texture& tex);
	void setupTextureEnvState(MTL::RenderCommandEncoder* encoder);
	void bindTexturesToSlots(MTL::RenderCommandEncoder* encoder);
};
