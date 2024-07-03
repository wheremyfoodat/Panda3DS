#pragma once

#include "pica_to_mtl.hpp"

using namespace PICA;

namespace Metal {

struct PipelineHash {
    // Formats
    ColorFmt colorFmt;
    DepthFmt depthFmt;

    // Blending
    bool blendEnabled;
    u32 blendControl;
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
        u64 intHash = ((u64)hash.colorFmt << 36) | ((u64)hash.depthFmt << 33) | ((u64)hash.blendEnabled << 32) | (u64)hash.blendControl;
        auto& pipeline = pipelineCache[intHash];
        if (!pipeline) {
            MTL::RenderPipelineDescriptor* desc = MTL::RenderPipelineDescriptor::alloc()->init();
            desc->setVertexFunction(vertexFunction);
            desc->setFragmentFunction(fragmentFunction);
            desc->setVertexDescriptor(vertexDescriptor);

            auto colorAttachment = desc->colorAttachments()->object(0);
            colorAttachment->setPixelFormat(toMTLPixelFormatColor(hash.colorFmt));
            if (hash.blendEnabled) {
                const u8 rgbEquation = hash.blendControl & 0x7;
               	const u8 alphaEquation = Helpers::getBits<8, 3>(hash.blendControl);

               	// Get blending functions
               	const u8 rgbSourceFunc = Helpers::getBits<16, 4>(hash.blendControl);
               	const u8 rgbDestFunc = Helpers::getBits<20, 4>(hash.blendControl);
               	const u8 alphaSourceFunc = Helpers::getBits<24, 4>(hash.blendControl);
               	const u8 alphaDestFunc = Helpers::getBits<28, 4>(hash.blendControl);

                colorAttachment->setBlendingEnabled(true);
                colorAttachment->setRgbBlendOperation(toMTLBlendOperation(rgbEquation));
                colorAttachment->setAlphaBlendOperation(toMTLBlendOperation(alphaEquation));
               	colorAttachment->setSourceRGBBlendFactor(toMTLBlendFactor(rgbSourceFunc));
               	colorAttachment->setDestinationRGBBlendFactor(toMTLBlendFactor(rgbDestFunc));
               	colorAttachment->setSourceAlphaBlendFactor(toMTLBlendFactor(alphaSourceFunc));
               	colorAttachment->setDestinationAlphaBlendFactor(toMTLBlendFactor(alphaDestFunc));
            }

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
    std::unordered_map<u64, MTL::RenderPipelineState*> pipelineCache;

    MTL::Device* device;
    MTL::Function* vertexFunction;
    MTL::Function* fragmentFunction;
    MTL::VertexDescriptor* vertexDescriptor;
};

} // namespace Metal
