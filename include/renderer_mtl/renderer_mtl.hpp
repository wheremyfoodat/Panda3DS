#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include "renderer.hpp"
#include "mtl_texture.hpp"
#include "mtl_render_target.hpp"
#include "mtl_blit_pipeline_cache.hpp"
#include "mtl_draw_pipeline_cache.hpp"
#include "mtl_depth_stencil_cache.hpp"
#include "mtl_vertex_buffer_cache.hpp"
// HACK: use the OpenGL cache
#include "../renderer_gl/surface_cache.hpp"

class GPU;

namespace Metal {

struct ColorClearOp {
    MTL::Texture* texture;
    float r, g, b, a;
};

struct DepthClearOp {
    MTL::Texture* texture;
    float depth;
};

struct StencilClearOp {
    MTL::Texture* texture;
    u8 stencil;
};

} // namespace Metal

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
	Metal::BlitPipelineCache blitPipelineCache;
	Metal::DrawPipelineCache drawPipelineCache;
	Metal::DepthStencilCache depthStencilCache;
	Metal::VertexBufferCache vertexBufferCache;

	// Objects
	MTL::SamplerState* nearestSampler;
	MTL::SamplerState* linearSampler;
	MTL::Texture* lightLUTTextureArray;
	MTL::DepthStencilState* defaultDepthStencilState;

	// Pipelines
	MTL::RenderPipelineState* displayPipeline;
	MTL::RenderPipelineState* copyToLutTexturePipeline;

	// Clears
	std::vector<Metal::ColorClearOp> colorClearOps;
	std::vector<Metal::DepthClearOp> depthClearOps;
	std::vector<Metal::StencilClearOp> stencilClearOps;

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

	void beginRenderPassIfNeeded(MTL::RenderPassDescriptor* renderPassDescriptor, bool doesClears, MTL::Texture* colorTexture, MTL::Texture* depthTexture = nullptr) {
		createCommandBufferIfNeeded();

		if (doesClears || !renderCommandEncoder || colorTexture != lastColorTexture || (depthTexture != lastDepthTexture || depthTexture == nullptr)) {
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

    void clearColor(MTL::RenderPassDescriptor* renderPassDescriptor, Metal::ColorClearOp clearOp) {
        bool beginRenderPass = (renderPassDescriptor == nullptr);
        if (!renderPassDescriptor) {
            renderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();
        }
		MTL::RenderPassColorAttachmentDescriptor* colorAttachment = renderPassDescriptor->colorAttachments()->object(0);
		colorAttachment->setTexture(clearOp.texture);
		colorAttachment->setClearColor(MTL::ClearColor(clearOp.r, clearOp.g, clearOp.b, clearOp.a));
		colorAttachment->setLoadAction(MTL::LoadActionClear);
		colorAttachment->setStoreAction(MTL::StoreActionStore);

		if (beginRenderPass) {
            beginRenderPassIfNeeded(renderPassDescriptor, true, clearOp.texture);
		}
    }

    bool clearColor(MTL::RenderPassDescriptor* renderPassDescriptor, MTL::Texture* texture) {
        for (int32_t i = colorClearOps.size() - 1; i >= 0; i--) {
            if (colorClearOps[i].texture == texture) {
                clearColor(renderPassDescriptor, colorClearOps[i]);
                colorClearOps.erase(colorClearOps.begin() + i);
                return true;
            }
        }

        if (renderPassDescriptor) {
            MTL::RenderPassColorAttachmentDescriptor* colorAttachment = renderPassDescriptor->colorAttachments()->object(0);
    		colorAttachment->setTexture(texture);
    		colorAttachment->setLoadAction(MTL::LoadActionLoad);
    		colorAttachment->setStoreAction(MTL::StoreActionStore);
        }

        return false;
    }

    void clearDepth(MTL::RenderPassDescriptor* renderPassDescriptor, Metal::DepthClearOp clearOp) {
        bool beginRenderPass = (renderPassDescriptor == nullptr);
        if (!renderPassDescriptor) {
            renderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();
        }
        MTL::RenderPassDepthAttachmentDescriptor* depthAttachment = renderPassDescriptor->depthAttachment();
        depthAttachment->setTexture(clearOp.texture);
        depthAttachment->setClearDepth(clearOp.depth);
        depthAttachment->setLoadAction(MTL::LoadActionClear);
        depthAttachment->setStoreAction(MTL::StoreActionStore);

        if (beginRenderPass) {
            beginRenderPassIfNeeded(renderPassDescriptor, true, nullptr, clearOp.texture);
        }
    }

    bool clearDepth(MTL::RenderPassDescriptor* renderPassDescriptor, MTL::Texture* texture) {
        for (int32_t i = depthClearOps.size() - 1; i >= 0; i--) {
            if (depthClearOps[i].texture == texture) {
                clearDepth(renderPassDescriptor, depthClearOps[i]);
                depthClearOps.erase(depthClearOps.begin() + i);
                return true;
            }
        }

        if (renderPassDescriptor) {
            MTL::RenderPassDepthAttachmentDescriptor* depthAttachment = renderPassDescriptor->depthAttachment();
    		depthAttachment->setTexture(texture);
    		depthAttachment->setLoadAction(MTL::LoadActionLoad);
    		depthAttachment->setStoreAction(MTL::StoreActionStore);
        }

        return false;
    }

    void clearStencil(MTL::RenderPassDescriptor* renderPassDescriptor, Metal::StencilClearOp clearOp) {
        bool beginRenderPass = (renderPassDescriptor == nullptr);
        if (!renderPassDescriptor) {
            renderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();
        }
        MTL::RenderPassStencilAttachmentDescriptor* stencilAttachment = renderPassDescriptor->stencilAttachment();
        stencilAttachment->setTexture(clearOp.texture);
        stencilAttachment->setClearStencil(clearOp.stencil);
        stencilAttachment->setLoadAction(MTL::LoadActionClear);
        stencilAttachment->setStoreAction(MTL::StoreActionStore);

        if (beginRenderPass) {
            beginRenderPassIfNeeded(renderPassDescriptor, true, nullptr, clearOp.texture);
        }
    }

    bool clearStencil(MTL::RenderPassDescriptor* renderPassDescriptor, MTL::Texture* texture) {
        for (int32_t i = stencilClearOps.size() - 1; i >= 0; i--) {
            if (stencilClearOps[i].texture == texture) {
                clearStencil(renderPassDescriptor, stencilClearOps[i]);
                stencilClearOps.erase(stencilClearOps.begin() + i);
                return true;
            }
        }

        if (renderPassDescriptor) {
            MTL::RenderPassStencilAttachmentDescriptor* stencilAttachment = renderPassDescriptor->stencilAttachment();
    		stencilAttachment->setTexture(texture);
    		stencilAttachment->setLoadAction(MTL::LoadActionLoad);
    		stencilAttachment->setStoreAction(MTL::StoreActionStore);
        }

        return false;
    }

	std::optional<Metal::ColorRenderTarget> getColorRenderTarget(u32 addr, PICA::ColorFmt format, u32 width, u32 height, bool createIfnotFound = true);
	Metal::DepthStencilRenderTarget& getDepthRenderTarget();
	Metal::Texture& getTexture(Metal::Texture& tex);
	void setupTextureEnvState(MTL::RenderCommandEncoder* encoder);
	void bindTexturesToSlots(MTL::RenderCommandEncoder* encoder);
	void updateLightingLUT(MTL::RenderCommandEncoder* encoder);
};
