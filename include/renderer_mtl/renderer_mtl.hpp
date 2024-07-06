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

struct Color4 {
    float r, g, b, a;
};

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
	std::map<MTL::Texture*, Color4> colorClearOps;
	std::map<MTL::Texture*, float> depthClearOps;
	std::map<MTL::Texture*, u8> stencilClearOps;

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

    template<typename AttachmentT, typename ClearDataT, typename GetAttachmentT, typename SetClearDataT>
    inline void clearAttachment(MTL::RenderPassDescriptor* renderPassDescriptor, MTL::Texture* texture, ClearDataT clearData, GetAttachmentT getAttachment, SetClearDataT setClearData) {
        bool beginRenderPass = (renderPassDescriptor == nullptr);
        if (!renderPassDescriptor) {
            renderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();
        }

        AttachmentT* attachment = getAttachment();
		attachment->setTexture(texture);
		setClearData(attachment, clearData);
		attachment->setLoadAction(MTL::LoadActionClear);
		attachment->setStoreAction(MTL::StoreActionStore);

		if (beginRenderPass) {
		    if (std::is_same<AttachmentT, MTL::RenderPassColorAttachmentDescriptor>::value)
                beginRenderPassIfNeeded(renderPassDescriptor, true, texture);
            else
                beginRenderPassIfNeeded(renderPassDescriptor, true, nullptr, texture);
		}
    }

    template<typename AttachmentT, typename ClearDataT, typename GetAttachmentT, typename SetClearDataT>
    inline bool clearAttachment(MTL::RenderPassDescriptor* renderPassDescriptor, MTL::Texture* texture, std::map<MTL::Texture*, ClearDataT>& clearOps, GetAttachmentT getAttachment, SetClearDataT setClearData) {
        auto it = clearOps.find(texture);
        if (it != clearOps.end()) {
            clearAttachment<AttachmentT>(renderPassDescriptor, texture, it->second, getAttachment, setClearData);
            clearOps.erase(it);
            return true;
        }

        if (renderPassDescriptor) {
            AttachmentT* attachment = getAttachment();
    		attachment->setTexture(texture);
    		attachment->setLoadAction(MTL::LoadActionLoad);
    		attachment->setStoreAction(MTL::StoreActionStore);
        }

        return false;
    }

    bool clearColor(MTL::RenderPassDescriptor* renderPassDescriptor, MTL::Texture* texture) {
        return clearAttachment<MTL::RenderPassColorAttachmentDescriptor, Color4>(renderPassDescriptor, texture, colorClearOps, [&]() { return renderPassDescriptor->colorAttachments()->object(0); }, [&](auto attachment, auto& color) {
            attachment->setClearColor(MTL::ClearColor(color.r, color.g, color.b, color.a));
        });
    }

    bool clearDepth(MTL::RenderPassDescriptor* renderPassDescriptor, MTL::Texture* texture) {
        return clearAttachment<MTL::RenderPassDepthAttachmentDescriptor, float>(renderPassDescriptor, texture, depthClearOps, [&]() { return renderPassDescriptor->depthAttachment(); }, [&](auto attachment, auto& depth) {
            attachment->setClearDepth(depth);
        });
    }

    bool clearStencil(MTL::RenderPassDescriptor* renderPassDescriptor, MTL::Texture* texture) {
        return clearAttachment<MTL::RenderPassStencilAttachmentDescriptor, u8>(renderPassDescriptor, texture, stencilClearOps, [&]() { return renderPassDescriptor->stencilAttachment(); }, [&](auto attachment, auto& stencil) {
            attachment->setClearStencil(stencil);
        });
    }

	std::optional<Metal::ColorRenderTarget> getColorRenderTarget(u32 addr, PICA::ColorFmt format, u32 width, u32 height, bool createIfnotFound = true);
	Metal::DepthStencilRenderTarget& getDepthRenderTarget();
	Metal::Texture& getTexture(Metal::Texture& tex);
	void setupTextureEnvState(MTL::RenderCommandEncoder* encoder);
	void bindTexturesToSlots(MTL::RenderCommandEncoder* encoder);
	void updateLightingLUT(MTL::RenderCommandEncoder* encoder);
};
