#include "renderer_mtl/mtl_texture.hpp"

#include <array>

#include "colour.hpp"
#include "renderer_mtl/objc_helper.hpp"


using namespace Helpers;

namespace Metal {
	void Texture::allocate() {
		formatInfo = PICA::getPixelFormatInfo(format);

		MTL::TextureDescriptor* descriptor = MTL::TextureDescriptor::alloc()->init();
		descriptor->setTextureType(MTL::TextureType2D);
		descriptor->setPixelFormat(formatInfo.pixelFormat);
		descriptor->setWidth(size.u());
		descriptor->setHeight(size.v());
		descriptor->setUsage(MTL::TextureUsageShaderRead);
		descriptor->setStorageMode(MTL::StorageModeShared);  // TODO: use private + staging buffers?
		texture = device->newTexture(descriptor);
		texture->setLabel(toNSString(
			"Texture " + std::string(PICA::textureFormatToString(format)) + " " + std::to_string(size.u()) + "x" + std::to_string(size.v())
		));
		descriptor->release();

		setNewConfig(config);
	}

	// Set the texture's configuration, which includes min/mag filters, wrapping S/T modes, and so on
	void Texture::setNewConfig(u32 cfg) {
		config = cfg;

		if (sampler) {
			sampler->release();
		}

		const auto magFilter = (cfg & 0x2) != 0 ? MTL::SamplerMinMagFilterLinear : MTL::SamplerMinMagFilterNearest;
		const auto minFilter = (cfg & 0x4) != 0 ? MTL::SamplerMinMagFilterLinear : MTL::SamplerMinMagFilterNearest;
		const auto wrapT = PICA::toMTLSamplerAddressMode(getBits<8, 3>(cfg));
		const auto wrapS = PICA::toMTLSamplerAddressMode(getBits<12, 3>(cfg));

		MTL::SamplerDescriptor* samplerDescriptor = MTL::SamplerDescriptor::alloc()->init();
		samplerDescriptor->setMinFilter(minFilter);
		samplerDescriptor->setMagFilter(magFilter);
		samplerDescriptor->setSAddressMode(wrapS);
		samplerDescriptor->setTAddressMode(wrapT);

		samplerDescriptor->setLabel(toNSString("Sampler"));
		sampler = device->newSamplerState(samplerDescriptor);
		samplerDescriptor->release();
	}

	void Texture::free() {
		valid = false;

		if (texture) {
			texture->release();
		}
		if (sampler) {
			sampler->release();
		}
	}

	u64 Texture::sizeInBytes() {
		u64 pixelCount = u64(size.x()) * u64(size.y());

		switch (format) {
			case PICA::TextureFmt::RGBA8:  // 4 bytes per pixel
				return pixelCount * 4;

			case PICA::TextureFmt::RGB8:  // 3 bytes per pixel
				return pixelCount * 3;

			case PICA::TextureFmt::RGBA5551:  // 2 bytes per pixel
			case PICA::TextureFmt::RGB565:
			case PICA::TextureFmt::RGBA4:
			case PICA::TextureFmt::RG8:
			case PICA::TextureFmt::IA8: return pixelCount * 2;

			case PICA::TextureFmt::A8:  // 1 byte per pixel
			case PICA::TextureFmt::I8:
			case PICA::TextureFmt::IA4: return pixelCount;

			case PICA::TextureFmt::I4:  // 4 bits per pixel
			case PICA::TextureFmt::A4: return pixelCount / 2;

			case PICA::TextureFmt::ETC1:  // Compressed formats
			case PICA::TextureFmt::ETC1A4: {
				// Number of 4x4 tiles
				const u64 tileCount = pixelCount / 16;
				// Tiles are 8 bytes each on ETC1 and 16 bytes each on ETC1A4
				const u64 tileSize = format == PICA::TextureFmt::ETC1 ? 8 : 16;
				return tileCount * tileSize;
			}

			default: Helpers::panic("[PICA] Attempted to get size of invalid texture type");
		}
	}

	// u and v are the UVs of the relevant texel
	// Texture data is stored interleaved in Morton order, ie in a Z - order curve as shown here
	// https://en.wikipedia.org/wiki/Z-order_curve
	// Textures are split into 8x8 tiles.This function returns the in - tile offset depending on the u & v of the texel
	// The in - tile offset is the sum of 2 offsets, one depending on the value of u % 8 and the other on the value of y % 8
	// As documented in this picture https ://en.wikipedia.org/wiki/File:Moser%E2%80%93de_Bruijn_addition.svg
	u32 Texture::mortonInterleave(u32 u, u32 v) {
		static constexpr u32 xOffsets[] = {0, 1, 4, 5, 16, 17, 20, 21};
		static constexpr u32 yOffsets[] = {0, 2, 8, 10, 32, 34, 40, 42};

		return xOffsets[u & 7] + yOffsets[v & 7];
	}

	// Get the byte offset of texel (u, v) in the texture
	u32 Texture::getSwizzledOffset(u32 u, u32 v, u32 width, u32 bytesPerPixel) {
		u32 offset = ((u & ~7) * 8) + ((v & ~7) * width);  // Offset of the 8x8 tile the texel belongs to
		offset += mortonInterleave(u, v);                  // Add the in-tile offset of the texel

		return offset * bytesPerPixel;
	}

	// Same as the above code except we need to divide by 2 because 4 bits is smaller than a byte
	u32 Texture::getSwizzledOffset_4bpp(u32 u, u32 v, u32 width) {
		u32 offset = ((u & ~7) * 8) + ((v & ~7) * width);  // Offset of the 8x8 tile the texel belongs to
		offset += mortonInterleave(u, v);                  // Add the in-tile offset of the texel

		return offset / 2;
	}

	u8 Texture::decodeTexelU8(u32 u, u32 v, PICA::TextureFmt fmt, std::span<const u8> data) {
		switch (fmt) {
			case PICA::TextureFmt::A4: {
				const u32 offset = getSwizzledOffset_4bpp(u, v, size.u());

				// For odd U coordinates, grab the top 4 bits, and the low 4 bits for even coordinates
				u8 alpha = data[offset] >> ((u % 2) ? 4 : 0);
				alpha = Colour::convert4To8Bit(getBits<0, 4>(alpha));

				// A8
				return alpha;
			}

			case PICA::TextureFmt::A8: {
				u32 offset = getSwizzledOffset(u, v, size.u(), 1);
				const u8 alpha = data[offset];

				// A8
				return alpha;
			}

			default: Helpers::panic("[Texture::DecodeTexel] Unimplemented format = %d", static_cast<int>(fmt));
		}
	}

	u16 Texture::decodeTexelU16(u32 u, u32 v, PICA::TextureFmt fmt, std::span<const u8> data) {
		switch (fmt) {
			case PICA::TextureFmt::RG8: {
				u32 offset = getSwizzledOffset(u, v, size.u(), 2);
				constexpr u8 b = 0;
				const u8 g = data[offset];
				const u8 r = data[offset + 1];

				// RG8
				return (g << 8) | r;
			}

			case PICA::TextureFmt::RGBA4: {
				u32 offset = getSwizzledOffset(u, v, size.u(), 2);
				u16 texel = u16(data[offset]) | (u16(data[offset + 1]) << 8);

				u8 alpha = getBits<0, 4, u8>(texel);
				u8 b = getBits<4, 4, u8>(texel);
				u8 g = getBits<8, 4, u8>(texel);
				u8 r = getBits<12, 4, u8>(texel);

				// ABGR4
				return (r << 12) | (g << 8) | (b << 4) | alpha;
			}

			case PICA::TextureFmt::RGBA5551: {
				const u32 offset = getSwizzledOffset(u, v, size.u(), 2);
				const u16 texel = u16(data[offset]) | (u16(data[offset + 1]) << 8);

				u8 alpha = getBit<0>(texel) ? 0xff : 0;
				u8 b = getBits<1, 5, u8>(texel);
				u8 g = getBits<6, 5, u8>(texel);
				u8 r = getBits<11, 5, u8>(texel);

				// BGR5A1
				return (alpha << 15) | (r << 10) | (g << 5) | b;
			}

			case PICA::TextureFmt::RGB565: {
				const u32 offset = getSwizzledOffset(u, v, size.u(), 2);
				const u16 texel = u16(data[offset]) | (u16(data[offset + 1]) << 8);

				const u8 b = getBits<0, 5, u8>(texel);
				const u8 g = getBits<5, 6, u8>(texel);
				const u8 r = getBits<11, 5, u8>(texel);

				// B5G6R5
				return (r << 11) | (g << 5) | b;
			}

			case PICA::TextureFmt::IA4: {
				const u32 offset = getSwizzledOffset(u, v, size.u(), 1);
				const u8 texel = data[offset];
				const u8 alpha = texel & 0xf;
				const u8 intensity = texel >> 4;

				// ABGR4
				return (intensity << 12) | (intensity << 8) | (intensity << 4) | alpha;
			}

			case PICA::TextureFmt::I4: {
				u32 offset = getSwizzledOffset_4bpp(u, v, size.u());

				// For odd U coordinates, grab the top 4 bits, and the low 4 bits for even coordinates
				u8 intensity = data[offset] >> ((u % 2) ? 4 : 0);
				intensity = getBits<0, 4>(intensity);

				// ABGR4
				return (intensity << 12) | (intensity << 8) | (intensity << 4) | 0xff;
			}

			default: Helpers::panic("[Texture::DecodeTexel] Unimplemented format = %d", static_cast<int>(fmt));
		}
	}

	u32 Texture::decodeTexelU32(u32 u, u32 v, PICA::TextureFmt fmt, std::span<const u8> data) {
		switch (fmt) {
			case PICA::TextureFmt::RGB8: {
				const u32 offset = getSwizzledOffset(u, v, size.u(), 3);
				const u8 b = data[offset];
				const u8 g = data[offset + 1];
				const u8 r = data[offset + 2];

				// RGBA8
				return (0xff << 24) | (b << 16) | (g << 8) | r;
			}

			case PICA::TextureFmt::RGBA8: {
				const u32 offset = getSwizzledOffset(u, v, size.u(), 4);
				const u8 alpha = data[offset];
				const u8 b = data[offset + 1];
				const u8 g = data[offset + 2];
				const u8 r = data[offset + 3];

				// RGBA8
				return (alpha << 24) | (b << 16) | (g << 8) | r;
			}

			case PICA::TextureFmt::I8: {
				u32 offset = getSwizzledOffset(u, v, size.u(), 1);
				const u8 intensity = data[offset];

				// RGBA8
				return (0xff << 24) | (intensity << 16) | (intensity << 8) | intensity;
			}

			case PICA::TextureFmt::IA8: {
				u32 offset = getSwizzledOffset(u, v, size.u(), 2);

				// Same as I8 except each pixel gets its own alpha value too
				const u8 alpha = data[offset];
				const u8 intensity = data[offset + 1];

				// RGBA8
				return (alpha << 24) | (intensity << 16) | (intensity << 8) | intensity;
			}

			case PICA::TextureFmt::ETC1: return getTexelETC(false, u, v, size.u(), data);
			case PICA::TextureFmt::ETC1A4: return getTexelETC(true, u, v, size.u(), data);

			default: Helpers::panic("[Texture::DecodeTexel] Unimplemented format = %d", static_cast<int>(fmt));
		}
	}

	void Texture::decodeTexture(std::span<const u8> data) {
		std::vector<u8> decoded;
		decoded.reserve(u64(size.u()) * u64(size.v()) * formatInfo.bytesPerTexel);

		// Decode texels line by line
		for (u32 v = 0; v < size.v(); v++) {
			for (u32 u = 0; u < size.u(); u++) {
				if (formatInfo.bytesPerTexel == 1) {
					u8 texel = decodeTexelU8(u, v, format, data);
					decoded.push_back(texel);
				} else if (formatInfo.bytesPerTexel == 2) {
					u16 texel = decodeTexelU16(u, v, format, data);
					decoded.push_back((texel & 0x00ff) >> 0);
					decoded.push_back((texel & 0xff00) >> 8);
				} else if (formatInfo.bytesPerTexel == 4) {
					u32 texel = decodeTexelU32(u, v, format, data);
					decoded.push_back((texel & 0x000000ff) >> 0);
					decoded.push_back((texel & 0x0000ff00) >> 8);
					decoded.push_back((texel & 0x00ff0000) >> 16);
					decoded.push_back((texel & 0xff000000) >> 24);
				} else {
					Helpers::panic("[Texture::decodeTexture] Unimplemented bytesPerTexel (%u)", formatInfo.bytesPerTexel);
				}
			}
		}

		texture->replaceRegion(MTL::Region(0, 0, size.u(), size.v()), 0, 0, decoded.data(), formatInfo.bytesPerTexel * size.u(), 0);
	}
}  // namespace Metal
