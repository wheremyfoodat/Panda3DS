#pragma once
#include <Metal/Metal.hpp>
#include <array>
#include <string>

#include "boost/icl/interval.hpp"
#include "helpers.hpp"
#include "math_util.hpp"
#include "objc_helper.hpp"
#include "opengl.hpp"
#include "pica_to_mtl.hpp"

template <typename T>
using Interval = boost::icl::right_open_interval<T>;

namespace Metal {
	template <typename Format_t>
	struct RenderTarget {
		MTL::Device* device;

		u32 location;
		Format_t format;
		OpenGL::uvec2 size;
		bool valid;

		// Range of VRAM taken up by buffer
		Interval<u32> range;

		MTL::Texture* texture = nullptr;

		RenderTarget() : valid(false) {}

		RenderTarget(MTL::Device* dev, u32 loc, Format_t format, u32 x, u32 y, bool valid = true)
			: device(dev), location(loc), format(format), size({x, y}), valid(valid) {
			u64 endLoc = (u64)loc + sizeInBytes();
			// Check if start and end are valid here
			range = Interval<u32>(loc, (u32)endLoc);
		}

		Math::Rect<u32> getSubRect(u32 inputAddress, u32 width, u32 height) {
			const u32 startOffset = (inputAddress - location) / sizePerPixel(format);
			const u32 x0 = (startOffset % (size.x() * 8)) / 8;
			const u32 y0 = (startOffset / (size.x() * 8)) * 8;
			return Math::Rect<u32>{x0, size.y() - y0, x0 + width, size.y() - height - y0};
		}

		// For 2 textures to "match" we only care about their locations, formats, and dimensions to match
		// For other things, such as filtering mode, etc, we can just switch the attributes of the cached texture
		bool matches(RenderTarget& other) {
			return location == other.location && format == other.format && size.x() == other.size.x() && size.y() == other.size.y();
		}

		void allocate() {
			MTL::PixelFormat pixelFormat = MTL::PixelFormatInvalid;
			if (std::is_same<Format_t, PICA::ColorFmt>::value) {
				pixelFormat = PICA::toMTLPixelFormatColor((PICA::ColorFmt)format);
			} else if (std::is_same<Format_t, PICA::DepthFmt>::value) {
				pixelFormat = PICA::toMTLPixelFormatDepth((PICA::DepthFmt)format);
			} else {
				Helpers::panic("Invalid format type");
			}

			MTL::TextureDescriptor* descriptor = MTL::TextureDescriptor::alloc()->init();
			descriptor->setTextureType(MTL::TextureType2D);
			descriptor->setPixelFormat(pixelFormat);
			descriptor->setWidth(size.u());
			descriptor->setHeight(size.v());
			descriptor->setUsage(MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead);
			descriptor->setStorageMode(MTL::StorageModePrivate);
			texture = device->newTexture(descriptor);
			texture->setLabel(toNSString(
				std::string(std::is_same<Format_t, PICA::ColorFmt>::value ? "Color" : "Depth") + " render target " + std::to_string(size.u()) + "x" +
				std::to_string(size.v())
			));
			descriptor->release();
		}

		void free() {
			valid = false;

			if (texture) {
				texture->release();
			}
		}

		u64 sizeInBytes() { return (size_t)size.x() * (size_t)size.y() * PICA::sizePerPixel(format); }
	};

	using ColorRenderTarget = RenderTarget<PICA::ColorFmt>;
	using DepthStencilRenderTarget = RenderTarget<PICA::DepthFmt>;
}  // namespace Metal
