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

} // namespace PICA
