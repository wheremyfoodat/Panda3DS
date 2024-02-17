#pragma once

#include <array>
#include <string>
#include <span>
#include "PICA/regs.hpp"
#include "boost/icl/right_open_interval.hpp"
#include "helpers.hpp"
#include "math_util.hpp"

template <typename T>
using Interval = boost::icl::right_open_interval<T>;

namespace PICA {

struct Texture {
    u32 location;
    u32 config; // Magnification/minification filter, wrapping configs, etc
    PICA::TextureFmt format;
    Math::uvec2 size;
    bool valid = false;

    // Range of VRAM taken up by buffer
    Interval<u32> range;

    Texture() = default;

    Texture(u32 loc, PICA::TextureFmt format, u32 x, u32 y, u32 config, bool valid = true)
        : location(loc), config(config), format(format), size({x, y}), valid(valid) {

        u64 endLoc = (u64)loc + sizeInBytes();
        // Check if start and end are valid here
        range = Interval<u32>(loc, (u32)endLoc);
    }

    // For 2 textures to "match" we only care about their locations, formats, and dimensions to match
    // For other things, such as filtering mode, etc, we can just switch the attributes of the cached texture
    bool matches(Texture& other) {
        return location == other.location && format == other.format &&
            size.x() == other.size.x() && size.y() == other.size.y();
    }

    std::vector<u32> decodeTexture(std::span<const u8> data);
    u64 sizeInBytes();

    u32 decodeTexel(u32 u, u32 v, PICA::TextureFmt fmt, std::span<const u8> data);

    // Get the morton interleave offset of a texel based on its U and V values
    static u32 mortonInterleave(u32 u, u32 v);
    // Get the byte offset of texel (u, v) in the texture
    static u32 getSwizzledOffset(u32 u, u32 v, u32 width, u32 bytesPerPixel);
    static u32 getSwizzledOffset_4bpp(u32 u, u32 v, u32 width);

    // Returns the format of this texture as a string
    std::string_view formatToString() {
        return PICA::textureFormatToString(format);
    }

    // Returns the texel at coordinates (u, v) of an ETC1(A4) texture
    // TODO: Make hasAlpha a template parameter
    u32 getTexelETC(bool hasAlpha, u32 u, u32 v, u32 width, std::span<const u8> data);
    u32 decodeETC(u32 alpha, u32 u, u32 v, u64 colourData);
};

template <bool topLeftOrigin = false>
struct ColourBuffer {
    u32 location;
    PICA::ColorFmt format;
    Math::uvec2 size;
    bool valid = false;

    // Range of VRAM taken up by buffer
    Interval<u32> range;

    ColourBuffer() = default;

    ColourBuffer(u32 loc, PICA::ColorFmt format, u32 x, u32 y, bool valid = true) : location(loc), format(format), size({x, y}), valid(valid) {
        u64 endLoc = (u64)loc + sizeInBytes();
        // Check if start and end are valid here
        range = Interval<u32>(loc, (u32)endLoc);
    }

    Math::Rect<u32> getSubRect(u32 inputAddress, u32 width, u32 height) const {
        // PICA textures have top-left origin while OpenGL has bottom-left origin.
        // Flip the rectangle on the x axis to account for this.
        const u32 startOffset = (inputAddress - location) / sizePerPixel(format);
        const u32 x0 = (startOffset % (size.x() * 8)) / 8;
        const u32 y0 = (startOffset / (size.x() * 8)) * 8;
        if constexpr (topLeftOrigin) {
            return Math::Rect<u32>{x0, y0, x0 + width, y0 + height};
        } else {
            return Math::Rect<u32>{x0, size.y() - y0, x0 + width, size.y() - height - y0};
        }
    }

    bool matches(ColourBuffer& other) {
        return location == other.location && format == other.format && size.x() == other.size.x() && size.y() == other.size.y();
    }

    size_t sizeInBytes() {
        return (size_t)size.x() * (size_t)size.y() * PICA::sizePerPixel(format);
    }
};

struct DepthBuffer {
    u32 location;
    PICA::DepthFmt format;
    Math::uvec2 size;  // Implicitly set to the size of the framebuffer
    bool valid = false;

    // Range of VRAM taken up by buffer
    Interval<u32> range;

    DepthBuffer() = default;

    DepthBuffer(u32 loc, PICA::DepthFmt format, u32 x, u32 y, bool valid = true) : location(loc), format(format), size({x, y}), valid(valid) {
        u64 endLoc = (u64)loc + sizeInBytes();
        // Check if start and end are valid here
        range = Interval<u32>(loc, (u32)endLoc);
    }

    bool matches(DepthBuffer& other) {
        return location == other.location && format == other.format && size.x() == other.size.x() && size.y() == other.size.y();
    }

    size_t sizeInBytes() {
        return (size_t)size.x() * (size_t)size.y() * PICA::sizePerPixel(format);
    }
};

} // namespace PICA
