#pragma once

#include "pica_to_mtl.hpp"

using namespace PICA;

namespace Metal {

struct PipelineHash {
    ColorFmt colorFmt;
    DepthFmt depthFmt;
};

// Bind the vertex buffer to binding 30 so that it doesn't occupy the lower indices
#define VERTEX_BUFFER_BINDING_INDEX 30

// This pipeline only caches the pipeline with all of its color and depth attachment variations
class PipelineCache {
public:
    PipelineCache() = default;

    ~PipelineCache() {
        clear();
        vertexDescriptor->release();
        vertexFunction->release();
        fragmentFunction->release();
    }

    void set(MTL::Device* dev, MTL::Function* vert, MTL::Function* frag, MTL::VertexDescriptor* vertDesc) {
        device = dev;
        vertexFunction = vert;
        fragmentFunction = frag;
        vertexDescriptor = vertDesc;
    }

    MTL::RenderPipelineState* get(PipelineHash hash) {
        u8 intHash = (u8)hash.colorFmt << 4 | (u8)hash.depthFmt;
        auto& pipeline = pipelineCache[intHash];
        if (!pipeline) {
            MTL::RenderPipelineDescriptor* desc = MTL::RenderPipelineDescriptor::alloc()->init();
            desc->setVertexFunction(vertexFunction);
            desc->setFragmentFunction(fragmentFunction);
            desc->setVertexDescriptor(vertexDescriptor);

            auto colorAttachment = desc->colorAttachments()->object(0);
            colorAttachment->setPixelFormat(toMTLPixelFormatColor(hash.colorFmt));
            colorAttachment->setBlendingEnabled(true);
           	colorAttachment->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
           	colorAttachment->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
           	colorAttachment->setSourceAlphaBlendFactor(MTL::BlendFactorSourceAlpha);
           	colorAttachment->setDestinationAlphaBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);

            desc->setDepthAttachmentPixelFormat(toMTLPixelFormatDepth(hash.depthFmt));

           	NS::Error* error = nullptr;
           	pipeline = device->newRenderPipelineState(desc, &error);
           	if (error) {
          		Helpers::panic("Error creating draw pipeline state: %s", error->description()->cString(NS::ASCIIStringEncoding));
           	}

            desc->release();
        }

        return pipeline;
    }

    void clear() {
        for (auto& pair : pipelineCache) {
            pair.second->release();
        }
        pipelineCache.clear();
    }

private:
    std::unordered_map<u8, MTL::RenderPipelineState*> pipelineCache;

    MTL::Device* device;
    MTL::Function* vertexFunction;
    MTL::Function* fragmentFunction;
    MTL::VertexDescriptor* vertexDescriptor;
};

} // namespace Metal
