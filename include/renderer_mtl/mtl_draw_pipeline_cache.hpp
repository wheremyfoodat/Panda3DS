#pragma once

#include <map>

#include "pica_to_mtl.hpp"

using namespace PICA;

namespace Metal {
	struct DrawFragmentFunctionHash {
		u32 lightingConfig1;   // 32 bits (TODO: check this)
		bool lightingEnabled;  // 1 bit
		u8 lightingNumLights;  // 3 bits
		//                                 |   ref    | func |  on  |
		u16 alphaControl;  // 12 bits (mask:  11111111   0111   0001)
	};

	inline bool operator<(const DrawFragmentFunctionHash& l, const DrawFragmentFunctionHash& r) {
		if (!l.lightingEnabled && r.lightingEnabled) return true;
		if (l.lightingNumLights < r.lightingNumLights) return true;
		if (l.lightingConfig1 < r.lightingConfig1) return true;
		if (l.alphaControl < r.alphaControl) return true;

		return false;
	}

	struct DrawPipelineHash {  // 56 bits
		// Formats
		ColorFmt colorFmt;  // 3 bits
		DepthFmt depthFmt;  // 3 bits

		// Blending
		bool blendEnabled;  // 1 bit
		//                                 |    functions     |   aeq    |   ceq    |
		u32 blendControl;   // 22 bits (mask:  1111111111111111   00000111   00000111)
		u8 colorWriteMask;  // 4 bits

		DrawFragmentFunctionHash fragHash;
	};

	inline bool operator<(const DrawPipelineHash& l, const DrawPipelineHash& r) {
		if ((u32)l.colorFmt < (u32)r.colorFmt) return true;
		if ((u32)l.depthFmt < (u32)r.depthFmt) return true;
		if (!l.blendEnabled && r.blendEnabled) return true;
		if (l.blendControl < r.blendControl) return true;
		if (l.colorWriteMask < r.colorWriteMask) return true;
		if (l.fragHash < r.fragHash) return true;

		return false;
	}

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
			auto& pipeline = pipelineCache[hash];

			if (!pipeline) {
				auto& fragmentFunction = fragmentFunctionCache[hash.fragHash];
				if (!fragmentFunction) {
					MTL::FunctionConstantValues* constants = MTL::FunctionConstantValues::alloc()->init();
					constants->setConstantValue(&hash.fragHash.lightingEnabled, MTL::DataTypeBool, NS::UInteger(0));
					constants->setConstantValue(&hash.fragHash.lightingNumLights, MTL::DataTypeUChar, NS::UInteger(1));
					constants->setConstantValue(&hash.fragHash.lightingConfig1, MTL::DataTypeUInt, NS::UInteger(2));
					constants->setConstantValue(&hash.fragHash.alphaControl, MTL::DataTypeUShort, NS::UInteger(3));

					NS::Error* error = nullptr;
					fragmentFunction = library->newFunction(NS::String::string("fragmentDraw", NS::ASCIIStringEncoding), constants, &error);
					if (error) {
						Helpers::panic("Error creating draw fragment function: %s", error->description()->cString(NS::ASCIIStringEncoding));
					}
					constants->release();
				}

				MTL::RenderPipelineDescriptor* desc = MTL::RenderPipelineDescriptor::alloc()->init();
				desc->setVertexFunction(vertexFunction);
				desc->setFragmentFunction(fragmentFunction);
				desc->setVertexDescriptor(vertexDescriptor);

				auto colorAttachment = desc->colorAttachments()->object(0);
				colorAttachment->setPixelFormat(toMTLPixelFormatColor(hash.colorFmt));
				MTL::ColorWriteMask writeMask = 0;
				if (hash.colorWriteMask & 0x1) writeMask |= MTL::ColorWriteMaskRed;
				if (hash.colorWriteMask & 0x2) writeMask |= MTL::ColorWriteMaskGreen;
				if (hash.colorWriteMask & 0x4) writeMask |= MTL::ColorWriteMaskBlue;
				if (hash.colorWriteMask & 0x8) writeMask |= MTL::ColorWriteMaskAlpha;
				colorAttachment->setWriteMask(writeMask);
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

				MTL::PixelFormat depthFormat = toMTLPixelFormatDepth(hash.depthFmt);
				desc->setDepthAttachmentPixelFormat(depthFormat);
				if (hash.depthFmt == DepthFmt::Depth24Stencil8) desc->setStencilAttachmentPixelFormat(depthFormat);

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
		std::map<DrawPipelineHash, MTL::RenderPipelineState*> pipelineCache;
		std::map<DrawFragmentFunctionHash, MTL::Function*> fragmentFunctionCache;

		MTL::Device* device;
		MTL::Library* library;
		MTL::Function* vertexFunction;
		MTL::VertexDescriptor* vertexDescriptor;
	};

}  // namespace Metal
