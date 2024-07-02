#pragma once
#include <array>
#include <string>
#include <Metal/Metal.hpp>
#include "PICA/regs.hpp"
#include "boost/icl/interval.hpp"
#include "helpers.hpp"
#include "math_util.hpp"
#include "opengl.hpp"

template <typename T>
using Interval = boost::icl::right_open_interval<T>;

namespace Metal {

struct RenderTarget {
    MTL::Device* device;

    u32 location;
    PICA::ColorFmt format;
    OpenGL::uvec2 size;
    bool valid;

    // Range of VRAM taken up by buffer
    Interval<u32> range;

    MTL::Texture* texture = nullptr;

    RenderTarget() : valid(false) {}

    RenderTarget(MTL::Device* dev, u32 loc, PICA::ColorFmt format, u32 x, u32 y, bool valid = true)
        : device(dev), location(loc), format(format), size({x, y}), valid(valid) {

        u64 endLoc = (u64)loc + sizeInBytes();
        // Check if start and end are valid here
        range = Interval<u32>(loc, (u32)endLoc);
    }

	Math::Rect<u32> getSubRect(u32 inputAddress, u32 width, u32 height) {
		const u32 startOffset = (inputAddress - location) / sizePerPixel(format);
		const u32 x0 = (startOffset % (size.x() * 8)) / 8;
		const u32 y0 = (startOffset / (size.x() * 8)) * 8;
		return Math::Rect<u32>{x0, y0, x0 + width, y0 + height};
	}

    // For 2 textures to "match" we only care about their locations, formats, and dimensions to match
    // For other things, such as filtering mode, etc, we can just switch the attributes of the cached texture
    bool matches(RenderTarget& other) {
        return location == other.location && format == other.format &&
            size.x() == other.size.x() && size.y() == other.size.y();
    }

    void allocate() {
        MTL::TextureDescriptor* descriptor = MTL::TextureDescriptor::alloc()->init();
        descriptor->setTextureType(MTL::TextureType2D);
        descriptor->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
        descriptor->setWidth(size.u());
        descriptor->setHeight(size.v());
        descriptor->setUsage(MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead);
        descriptor->setStorageMode(MTL::StorageModePrivate);
        texture = device->newTexture(descriptor);
    }

    void free() {
        valid = false;

    	if (texture) {
    		texture->release();
    	}
    }

    u64 sizeInBytes() {
        return (size_t)size.x() * (size_t)size.y() * PICA::sizePerPixel(format);
    }
};

} // namespace Metal
