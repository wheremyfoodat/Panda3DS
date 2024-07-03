#pragma once

#include <Metal/Metal.hpp>
#include "PICA/regs.hpp"

namespace PICA {

inline MTL::PixelFormat toMTLPixelFormatColor(ColorFmt format) {
    switch (format) {
    case ColorFmt::RGBA8: return MTL::PixelFormatRGBA8Unorm;
    case ColorFmt::RGB8: return MTL::PixelFormatRGBA8Unorm; // TODO: return the correct format
    case ColorFmt::RGBA5551: return MTL::PixelFormatBGR5A1Unorm;
    case ColorFmt::RGB565: return MTL::PixelFormatB5G6R5Unorm; // TODO: check if this is correct
    case ColorFmt::RGBA4: return MTL::PixelFormatABGR4Unorm; // TODO: check if this is correct
    }
}

inline MTL::PixelFormat toMTLPixelFormatDepth(DepthFmt format) {
    switch (format) {
    case DepthFmt::Depth16: return MTL::PixelFormatDepth16Unorm;
    case DepthFmt::Unknown1: return MTL::PixelFormatInvalid;
    case DepthFmt::Depth24: return MTL::PixelFormatDepth32Float; // TODO: is this okay?
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
    default: panic("Unknown compare function %u", func);
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
    case 5: return MTL::BlendOperationAdd; // Unused (same as 0)
    case 6: return MTL::BlendOperationAdd; // Unused (same as 0)
    case 7: return MTL::BlendOperationAdd; // Unused (same as 0)
    default: panic("Unknown blend operation %u", op);
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
    case 15: return MTL::BlendFactorOne; // Undocumented
    default: panic("Unknown blend factor %u", factor);
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
    default: panic("Unknown stencil operation %u", op);
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
        Helpers::warn("Geometry primitives are not yet, using triangles instead");
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
    default: panic("Unknown sampler address mode %u", addrMode);
    }

    return MTL::SamplerAddressModeClampToEdge;
}

} // namespace PICA
