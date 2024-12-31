#pragma once

#include <Metal/Metal.hpp>

#include "PICA/regs.hpp"

namespace PICA {
	struct PixelFormatInfo {
		MTL::PixelFormat pixelFormat;
		size_t bytesPerTexel;
	};

	constexpr PixelFormatInfo pixelFormatInfos[14] = {
		{MTL::PixelFormatRGBA8Unorm, 4},   // RGBA8
		{MTL::PixelFormatRGBA8Unorm, 4},   // RGB8
		{MTL::PixelFormatBGR5A1Unorm, 2},  // RGBA5551
		{MTL::PixelFormatB5G6R5Unorm, 2},  // RGB565
		{MTL::PixelFormatABGR4Unorm, 2},   // RGBA4
		{MTL::PixelFormatRGBA8Unorm, 4},   // IA8
		{MTL::PixelFormatRG8Unorm, 2},     // RG8
		{MTL::PixelFormatRGBA8Unorm, 4},   // I8
		{MTL::PixelFormatA8Unorm, 1},      // A8
		{MTL::PixelFormatABGR4Unorm, 2},   // IA4
		{MTL::PixelFormatABGR4Unorm, 2},   // I4
		{MTL::PixelFormatA8Unorm, 1},      // A4
		{MTL::PixelFormatRGBA8Unorm, 4},   // ETC1
		{MTL::PixelFormatRGBA8Unorm, 4},   // ETC1A4
	};

	inline PixelFormatInfo getPixelFormatInfo(TextureFmt format) { return pixelFormatInfos[static_cast<int>(format)]; }

	inline MTL::PixelFormat toMTLPixelFormatColor(ColorFmt format) {
		switch (format) {
			case ColorFmt::RGBA8: return MTL::PixelFormatRGBA8Unorm;
			case ColorFmt::RGB8: return MTL::PixelFormatRGBA8Unorm;
			case ColorFmt::RGBA5551: return MTL::PixelFormatRGBA8Unorm;  // TODO: use MTL::PixelFormatBGR5A1Unorm?
			case ColorFmt::RGB565: return MTL::PixelFormatRGBA8Unorm;    // TODO: use MTL::PixelFormatB5G6R5Unorm?
			case ColorFmt::RGBA4: return MTL::PixelFormatABGR4Unorm;
		}
	}

	inline MTL::PixelFormat toMTLPixelFormatDepth(DepthFmt format) {
		switch (format) {
			case DepthFmt::Depth16: return MTL::PixelFormatDepth16Unorm;
			case DepthFmt::Unknown1: return MTL::PixelFormatInvalid;
			case DepthFmt::Depth24:
				return MTL::PixelFormatDepth32Float;  // Metal does not support 24-bit depth formats
			// Apple sillicon doesn't support 24-bit depth buffers, so we use 32-bit instead
			case DepthFmt::Depth24Stencil8: return MTL::PixelFormatDepth32Float_Stencil8;
		}
	}

	inline MTL::CompareFunction toMTLCompareFunc(u8 func) {
		switch (func) {
			case 0: return MTL::CompareFunctionNever;
			case 1: return MTL::CompareFunctionAlways;
			case 2: return MTL::CompareFunctionEqual;
			case 3: return MTL::CompareFunctionNotEqual;
			case 4: return MTL::CompareFunctionLess;
			case 5: return MTL::CompareFunctionLessEqual;
			case 6: return MTL::CompareFunctionGreater;
			case 7: return MTL::CompareFunctionGreaterEqual;
			default: Helpers::panic("Unknown compare function %u", func);
		}

		return MTL::CompareFunctionAlways;
	}

	inline MTL::BlendOperation toMTLBlendOperation(u8 op) {
		switch (op) {
			case 0: return MTL::BlendOperationAdd;
			case 1: return MTL::BlendOperationSubtract;
			case 2: return MTL::BlendOperationReverseSubtract;
			case 3: return MTL::BlendOperationMin;
			case 4: return MTL::BlendOperationMax;
			case 5: return MTL::BlendOperationAdd;  // Unused (same as 0)
			case 6: return MTL::BlendOperationAdd;  // Unused (same as 0)
			case 7: return MTL::BlendOperationAdd;  // Unused (same as 0)
			default: Helpers::panic("Unknown blend operation %u", op);
		}

		return MTL::BlendOperationAdd;
	}

	inline MTL::BlendFactor toMTLBlendFactor(u8 factor) {
		switch (factor) {
			case 0: return MTL::BlendFactorZero;
			case 1: return MTL::BlendFactorOne;
			case 2: return MTL::BlendFactorSourceColor;
			case 3: return MTL::BlendFactorOneMinusSourceColor;
			case 4: return MTL::BlendFactorDestinationColor;
			case 5: return MTL::BlendFactorOneMinusDestinationColor;
			case 6: return MTL::BlendFactorSourceAlpha;
			case 7: return MTL::BlendFactorOneMinusSourceAlpha;
			case 8: return MTL::BlendFactorDestinationAlpha;
			case 9: return MTL::BlendFactorOneMinusDestinationAlpha;
			case 10: return MTL::BlendFactorBlendColor;
			case 11: return MTL::BlendFactorOneMinusBlendColor;
			case 12: return MTL::BlendFactorBlendAlpha;
			case 13: return MTL::BlendFactorOneMinusBlendAlpha;
			case 14: return MTL::BlendFactorSourceAlphaSaturated;
			case 15: return MTL::BlendFactorOne;  // Undocumented
			default: Helpers::panic("Unknown blend factor %u", factor);
		}

		return MTL::BlendFactorOne;
	}

	inline MTL::StencilOperation toMTLStencilOperation(u8 op) {
		switch (op) {
			case 0: return MTL::StencilOperationKeep;
			case 1: return MTL::StencilOperationZero;
			case 2: return MTL::StencilOperationReplace;
			case 3: return MTL::StencilOperationIncrementClamp;
			case 4: return MTL::StencilOperationDecrementClamp;
			case 5: return MTL::StencilOperationInvert;
			case 6: return MTL::StencilOperationIncrementWrap;
			case 7: return MTL::StencilOperationDecrementWrap;
			default: Helpers::panic("Unknown stencil operation %u", op);
		}

		return MTL::StencilOperationKeep;
	}

	inline MTL::PrimitiveType toMTLPrimitiveType(PrimType primType) {
		switch (primType) {
			case PrimType::TriangleList: return MTL::PrimitiveTypeTriangle;
			case PrimType::TriangleStrip: return MTL::PrimitiveTypeTriangleStrip;
			case PrimType::TriangleFan:
				Helpers::warn("Triangle fans are not supported on Metal, using triangles instead");
				return MTL::PrimitiveTypeTriangle;
			case PrimType::GeometryPrimitive:
				return MTL::PrimitiveTypeTriangle;
		}
	}

	inline MTL::SamplerAddressMode toMTLSamplerAddressMode(u8 addrMode) {
		switch (addrMode) {
			case 0: return MTL::SamplerAddressModeClampToEdge;
			case 1: return MTL::SamplerAddressModeClampToBorderColor;
			case 2: return MTL::SamplerAddressModeRepeat;
			case 3: return MTL::SamplerAddressModeMirrorRepeat;
			case 4: return MTL::SamplerAddressModeClampToEdge;
			case 5: return MTL::SamplerAddressModeClampToBorderColor;
			case 6: return MTL::SamplerAddressModeRepeat;
			case 7: return MTL::SamplerAddressModeRepeat;
			default: Helpers::panic("Unknown sampler address mode %u", addrMode);
		}

		return MTL::SamplerAddressModeClampToEdge;
	}
}  // namespace PICA
