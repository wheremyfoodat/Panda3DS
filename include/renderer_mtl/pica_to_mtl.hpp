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
    case DepthFmt::Depth24Stencil8: return MTL::PixelFormatDepth24Unorm_Stencil8;
    }
}

} // namespace PICA
