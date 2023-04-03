#pragma once
#include <array>
#include <string>
#include "boost/icl/interval.hpp"
#include "helpers.hpp"
#include "opengl.hpp"

template <typename T>
using Interval = boost::icl::right_open_interval<T>;

struct Texture {
    enum class Formats : u32 {
        RGBA8 = 0,
        RGB8 = 1,
        RGBA5551 = 2,
        RGB565 = 3,
        RGBA4 = 4,
        IA8 = 5,
        RG8 = 6,
        I8 = 7,
        A8 = 8,
        IA4 = 9,
        I4 = 10,
        A4 = 11,
        ETC1 = 12,
        ETC1A4 = 13,

        Trash1 = 14, Trash2 = 15 // TODO: What are these?
    };

    u32 location;
    u32 config; // Magnification/minification filter, wrapping configs, etc
    Formats format;
    OpenGL::uvec2 size;
    bool valid;

    // Range of VRAM taken up by buffer
    Interval<u32> range;
    // OpenGL resources allocated to buffer
    OpenGL::Texture texture;

    Texture() : valid(false) {}

    Texture(u32 loc, Formats format, u32 x, u32 y, u32 config, bool valid = true)
        : location(loc), format(format), size({x, y}), config(config), valid(valid) {

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

    void allocate();
    void setNewConfig(u32 newConfig);
    void decodeTexture(const void* data);
    void free();
    u64 sizeInBytes();

    u32 decodeTexel(u32 u, u32 v, Formats fmt, const void* data);

    // Get the morton interleave offset of a texel based on its U and V values
    static u32 mortonInterleave(u32 u, u32 v);
    // Get the byte offset of texel (u, v) in the texture
    static u32 getSwizzledOffset(u32 u, u32 v, u32 width, u32 bytesPerPixel);
    static u32 getSwizzledOffset_4bpp(u32 u, u32 v, u32 width);

    // Returns the string representation of a texture format
    static std::string textureFormatToString(Formats fmt);

    // Returns the format of this texture as a string
    std::string formatToString() {
        return textureFormatToString(format);
    }

    // Returns the texel at coordinates (u, v) of an ETC1(A4) texture
    // TODO: Make hasAlpha a template parameter
    u32 getTexelETC(bool hasAlpha, u32 u, u32 v, u32 width, const void* data);
    u32 decodeETC(u32 alpha, u32 u, u32 v, u64 colourData);
};