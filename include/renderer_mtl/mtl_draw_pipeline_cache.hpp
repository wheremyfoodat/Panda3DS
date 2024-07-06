#pragma once

#include "pica_to_mtl.hpp"

using namespace PICA;

namespace Metal {

struct DrawPipelineHash {
    // Formats
    ColorFmt colorFmt;
    DepthFmt depthFmt;

    // Blending
    bool blendEnabled;
    u32 blendControl;

    // Specialization constants
    bool lightingEnabled;
    u8 lightingNumLights;
    u8 lightingConfig1;
};

// Bind the vertex buffer to binding 30 so that it doesn't occupy the lower indices
#define VERTEX_BUFFER_BINDING_INDEX 30

// This pipeline only caches the pipeline with all of its color and depth attachment variations
class DrawPipelineCache {
public:
    DrawPipelineCache() = default;

    ~DrawPipelineCache() {
        clear();
        vertexDescriptor->release();
        vertexFunction->release();
    }

    void set(MTL::Device* dev, MTL::Library* lib, MTL::Function* vert, MTL::VertexDescriptor* vertDesc) {
        device = dev;
        library = lib;
        vertexFunction = vert;
        vertexDescriptor = vertDesc;
    }

    MTL::RenderPipelineState* get(DrawPipelineHash hash) {
        u16 fragmentFunctionHash = ((u8)hash.lightingEnabled << 12) | (hash.lightingNumLights << 8) | hash.lightingConfig1;
        u64 pipelineHash = ((u64)hash.colorFmt << 52) | ((u64)hash.depthFmt << 49) | ((u64)hash.blendEnabled << 48) | ((u64)hash.blendControl << 16) | fragmentFunctionHash;
        auto& pipeline = pipelineCache[pipelineHash];
        if (!pipeline) {
            auto& fragmentFunction = fragmentFunctionCache[fragmentFunctionHash];
            if (!fragmentFunction) {
                MTL::FunctionConstantValues* constants = MTL::FunctionConstantValues::alloc()->init();
                constants->setConstantValue(&hash.lightingEnabled, MTL::DataTypeBool, NS::UInteger(0));
                constants->setConstantValue(&hash.lightingNumLights, MTL::DataTypeUChar, NS::UInteger(1));
                constants->setConstantValue(&hash.lightingConfig1, MTL::DataTypeUChar, NS::UInteger(2));

                NS::Error* error = nullptr;
                fragmentFunction = library->newFunction(NS::String::string("fragmentDraw", NS::ASCIIStringEncoding), constants, &error);
                if (error) {
                    Helpers::panic("Error creating draw fragment function: %s", error->description()->cString(NS::ASCIIStringEncoding));
                }
                constants->release();
                fragmentFunctionCache[fragmentFunctionHash] = fragmentFunction;
            }

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
        for (auto& pair : fragmentFunctionCache) {
            pair.second->release();
        }
        fragmentFunctionCache.clear();
    }

private:
    std::unordered_map<u64, MTL::RenderPipelineState*> pipelineCache;
    std::unordered_map<u16, MTL::Function*> fragmentFunctionCache;

    MTL::Device* device;
    MTL::Library* library;
    MTL::Function* vertexFunction;
    MTL::VertexDescriptor* vertexDescriptor;
};

} // namespace Metal
