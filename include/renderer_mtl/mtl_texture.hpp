#pragma once

#include <Metal/Metal.hpp>
#include <array>
#include <string>

#include "PICA/regs.hpp"
#include "boost/icl/interval.hpp"
#include "helpers.hpp"
#include "math_util.hpp"
#include "opengl.hpp"
#include "renderer_mtl/pica_to_mtl.hpp"

template <typename T>
using Interval = boost::icl::right_open_interval<T>;

namespace Metal {
	struct Texture {
		MTL::Device* device;

		u32 location;
		u32 config;  // Magnification/minification filter, wrapping configs, etc
		PICA::TextureFmt format;
		OpenGL::uvec2 size;
		bool valid;

		// Range of VRAM taken up by buffer
		Interval<u32> range;

		PICA::PixelFormatInfo formatInfo;
		MTL::Texture* texture = nullptr;
		MTL::SamplerState* sampler = nullptr;

		Texture() : valid(false) {}

		Texture(MTL::Device* dev, u32 loc, PICA::TextureFmt format, u32 x, u32 y, u32 config, bool valid = true)
			: device(dev), location(loc), format(format), size({x, y}), config(config), valid(valid) {
			u64 endLoc = (u64)loc + sizeInBytes();
			// Check if start and end are valid here
			range = Interval<u32>(loc, (u32)endLoc);
		}

		// For 2 textures to "match" we only care about their locations, formats, and dimensions to match
		// For other things, such as filtering mode, etc, we can just switch the attributes of the cached texture
		bool matches(Texture& other) {
			return location == other.location && format == other.format && size.x() == other.size.x() && size.y() == other.size.y();
		}

		void allocate();
		void setNewConfig(u32 newConfig);
		void decodeTexture(std::span<const u8> data);
		void free();
		u64 sizeInBytes();

		u8 decodeTexelU8(u32 u, u32 v, PICA::TextureFmt fmt, std::span<const u8> data);
		u16 decodeTexelU16(u32 u, u32 v, PICA::TextureFmt fmt, std::span<const u8> data);
		u32 decodeTexelU32(u32 u, u32 v, PICA::TextureFmt fmt, std::span<const u8> data);

		// Get the morton interleave offset of a texel based on its U and V values
		static u32 mortonInterleave(u32 u, u32 v);
		// Get the byte offset of texel (u, v) in the texture
		static u32 getSwizzledOffset(u32 u, u32 v, u32 width, u32 bytesPerPixel);
		static u32 getSwizzledOffset_4bpp(u32 u, u32 v, u32 width);

		// Returns the format of this texture as a string
		std::string_view formatToString() { return PICA::textureFormatToString(format); }

		// Returns the texel at coordinates (u, v) of an ETC1(A4) texture
		// TODO: Make hasAlpha a template parameter
		u32 getTexelETC(bool hasAlpha, u32 u, u32 v, u32 width, std::span<const u8> data);
		u32 decodeETC(u32 alpha, u32 u, u32 v, u64 colourData);
	};
}  // namespace Metal
