#pragma once
#include "boost/icl/interval.hpp"
#include "helpers.hpp"
#include "opengl.hpp"

template <typename T>
using Interval = boost::icl::right_open_interval<T>;

struct ColourBuffer {
    enum class Formats : u32 {
        RGBA8 = 0,
        BGR8 = 1,
        RGB5A1 = 2,
        RGB565 = 3,
        RGBA4 = 4,
        
        Trash1 = 5, Trash2 = 6, Trash3 = 7 // Technically selectable, but their function is unknown
    };

    u32 location;
    Formats format;
    OpenGL::uvec2 size;
    bool valid;

    // Range of VRAM taken up by buffer
    Interval<u32> range;
    // OpenGL resources allocated to buffer
    OpenGL::Texture texture;
    OpenGL::Framebuffer fbo;

    ColourBuffer() : valid(false) {}

    ColourBuffer(u32 loc, Formats format, u32 x, u32 y, bool valid = true)
        : location(loc), format(format), size({x, y}), valid(valid) {

        u64 endLoc = (u64)loc + sizeInBytes();
        // Check if start and end are valid here
        range = Interval<u32>(loc, (u32)endLoc);
    }

    void allocate() {
        printf("Make this colour buffer allocate itself\n");
    }

    void free() {
        valid = false;
        printf("Make this colour buffer free itself\n");
    }

    bool matches(ColourBuffer& other) {
        return location == other.location && format == other.format &&
            size.x() == other.size.x() && size.y() == other.size.y();
    }

    // Size occupied by each pixel in bytes
    // All formats are 16BPP except for RGBA8 (32BPP) and BGR8 (24BPP)
    size_t sizePerPixel() {
        switch (format) {
            case Formats::BGR8: return 3;
            case Formats::RGBA8: return 4;
            default: return 2;
        }
    }

    size_t sizeInBytes() {
        return (size_t)size.x() * (size_t)size.y() * sizePerPixel();
    }
};

struct DepthBuffer {
    enum class Formats : u32 {
        Depth16 = 0,
        Garbage = 1,
        Depth24 = 2,
        Depth24Stencil8 = 3
    };

    u32 location;
    Formats format;
    OpenGL::uvec2 size; // Implicitly set to the size of the framebuffer
    bool valid;

    // Range of VRAM taken up by buffer
    Interval<u32> range;
    // OpenGL texture used for storing depth/stencil
    OpenGL::Texture texture;

    DepthBuffer() : valid(false) {}

    DepthBuffer(u32 loc, Formats format, u32 x, u32 y, bool valid = true) :
        location(loc), format(format), size({x, y}), valid(valid) {}

    bool hasStencil() {
        return format == Formats::Depth24Stencil8;
    }

    void allocate() {
        printf("Make this depth buffer allocate itself\n");
    }

    void free() {
        valid = false;
        printf("Make this depth buffer free itself\n");
    }

    bool matches(DepthBuffer& other) {
        return location == other.location && format == other.format &&
            size.x() == other.size.x() && size.y() == other.size.y();
    }

    // Size occupied by each pixel in bytes
    size_t sizePerPixel() {
        switch (format) {
            case Formats::Depth16: return 2;
            case Formats::Depth24: return 3;
            case Formats::Depth24Stencil8: return 4;

            default: return 1; // Invalid format
        }
    }

    size_t sizeInBytes() {
        return (size_t)size.x() * (size_t)size.y() * sizePerPixel();
    }
};