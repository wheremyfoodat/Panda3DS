#pragma once

#include <Metal/Metal.hpp>
#include <array>
#include <string>

#include "PICA/regs.hpp"
#include "boost/icl/interval.hpp"
#include "helpers.hpp"
#include "math_util.hpp"
#include "renderer_mtl/pica_to_mtl.hpp"
// TODO: remove dependency on OpenGL
#include "opengl.hpp"

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

		u8 decodeTexelBGR8ToRGBA8(u32 u, u32 v, std::span<const u8> data);
		u8 decodeTexelA1BGR5ToRGBA8(u32 u, u32 v, std::span<const u8> data);
		u8 decodeTexelB5G6R5ToRGBA8(u32 u, u32 v, std::span<const u8> data);
		u8 decodeTexelABGR4ToRGBA8(u32 u, u32 v, std::span<const u8> data);
		u8 decodeTexelAI8ToRGBA8(u32 u, u32 v, std::span<const u8> data);
		u8 decodeTexelI8ToRGBA8(u32 u, u32 v, std::span<const u8> data);
		u8 decodeTexelAI4ToRGBA4(u32 u, u32 v, std::span<const u8> data);
		u8 decodeTexelAI4ToRGBA8(u32 u, u32 v, std::span<const u8> data);
		u8 decodeTexelI4ToRGBA4(u32 u, u32 v, std::span<const u8> data);
		u8 decodeTexelI4ToRGBA8(u32 u, u32 v, std::span<const u8> data);
		u8 decodeTexelA4ToA8(u32 u, u32 v, std::span<const u8> data);

		// Returns the format of this texture as a string
		std::string_view formatToString() { return PICA::textureFormatToString(format); }
	};
}  // namespace Metal
