#pragma once

#include "pica_to_mtl.hpp"

using namespace PICA;

namespace Metal {

struct DrawPipelineHash { // 62 bits
    // Formats
    ColorFmt colorFmt; // 3 bits
    DepthFmt depthFmt; // 3 bits

    // Blending
    bool blendEnabled; // 1 bit
    u32 blendControl; // 32 bits

    // Specialization constants (23 bits)
    bool lightingEnabled; // 1 bit
    u8 lightingNumLights; // 3 bits
    u8 lightingConfig1; // 7 bits
    //                                 |   ref    | func |  on  |
    u16 alphaControl; // 12 bits (mask:  11111111   0111   0001)
};

// Bind the vertex buffer to binding 30 so that it doesn't occupy the lower indices
#define VERTEX_BUFFER_BINDING_INDEX 30

// This pipeline only caches the pipeline with all of its color and depth attachment variations
class DrawPipelineCache {
public:
    DrawPipelineCache() = default;

    ~DrawPipelineCache() {
        reset();
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
        u32 fragmentFunctionHash = ((u32)hash.lightingEnabled << 22) | ((u32)hash.lightingNumLights << 19) | ((u32)hash.lightingConfig1 << 12) | ((((u32)hash.alphaControl & 0b1111111100000000) >> 8) << 4) | ((((u32)hash.alphaControl & 0b01110000) >> 4) << 1) | ((u32)hash.alphaControl & 0b0001);
        u64 pipelineHash = ((u64)hash.colorFmt << 59) | ((u64)hash.depthFmt << 56) | ((u64)hash.blendEnabled << 55) | ((u64)hash.blendControl << 23) | fragmentFunctionHash;
        auto& pipeline = pipelineCache[pipelineHash];
        if (!pipeline) {
            auto& fragmentFunction = fragmentFunctionCache[fragmentFunctionHash];
            if (!fragmentFunction) {
                MTL::FunctionConstantValues* constants = MTL::FunctionConstantValues::alloc()->init();
                constants->setConstantValue(&hash.lightingEnabled, MTL::DataTypeBool, NS::UInteger(0));
                constants->setConstantValue(&hash.lightingNumLights, MTL::DataTypeUChar, NS::UInteger(1));
                constants->setConstantValue(&hash.lightingConfig1, MTL::DataTypeUChar, NS::UInteger(2));
                constants->setConstantValue(&hash.alphaControl, MTL::DataTypeUShort, NS::UInteger(3));

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
            desc->setLabel(toNSString("Draw pipeline"));
           	pipeline = device->newRenderPipelineState(desc, &error);
           	if (error) {
          		Helpers::panic("Error creating draw pipeline state: %s", error->description()->cString(NS::ASCIIStringEncoding));
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
        for (auto& pair : fragmentFunctionCache) {
            pair.second->release();
        }
        fragmentFunctionCache.clear();
    }

private:
    std::unordered_map<u64, MTL::RenderPipelineState*> pipelineCache;
    std::unordered_map<u32, MTL::Function*> fragmentFunctionCache;

    MTL::Device* device;
    MTL::Library* library;
    MTL::Function* vertexFunction;
    MTL::VertexDescriptor* vertexDescriptor;
};

} // namespace Metal
