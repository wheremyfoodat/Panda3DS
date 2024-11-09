#pragma once

#include <map>

#include "objc_helper.hpp"
#include "pica_to_mtl.hpp"

using namespace PICA;

namespace Metal {
	struct BlitPipelineHash {
		// Formats
		ColorFmt colorFmt;
		DepthFmt depthFmt;
	};

	// This pipeline only caches the pipeline with all of its color and depth attachment variations
	class BlitPipelineCache {
	  public:
		BlitPipelineCache() = default;

		~BlitPipelineCache() {
			reset();
			vertexFunction->release();
			fragmentFunction->release();
		}

		void set(MTL::Device* dev, MTL::Function* vert, MTL::Function* frag) {
			device = dev;
			vertexFunction = vert;
			fragmentFunction = frag;
		}

		MTL::RenderPipelineState* get(BlitPipelineHash hash) {
			u8 intHash = ((u8)hash.colorFmt << 3) | (u8)hash.depthFmt;
			auto& pipeline = pipelineCache[intHash];
			if (!pipeline) {
				MTL::RenderPipelineDescriptor* desc = MTL::RenderPipelineDescriptor::alloc()->init();
				desc->setVertexFunction(vertexFunction);
				desc->setFragmentFunction(fragmentFunction);

				auto colorAttachment = desc->colorAttachments()->object(0);
				colorAttachment->setPixelFormat(toMTLPixelFormatColor(hash.colorFmt));

				desc->setDepthAttachmentPixelFormat(toMTLPixelFormatDepth(hash.depthFmt));

				NS::Error* error = nullptr;
				desc->setLabel(toNSString("Blit pipeline"));
				pipeline = device->newRenderPipelineState(desc, &error);
				if (error) {
					Helpers::panic("Error creating blit pipeline state: %s", error->description()->cString(NS::ASCIIStringEncoding));
				}

				desc->release();
			}

			return pipeline;
		}

		void reset() {
			for (auto& pair : pipelineCache) {
				pair.second->release();
			}
			pipelineCache.clear();
		}

	  private:
		std::map<u8, MTL::RenderPipelineState*> pipelineCache;

		MTL::Device* device;
		MTL::Function* vertexFunction;
		MTL::Function* fragmentFunction;
	};
}  // namespace Metal
