#include "renderer_gl/textures.hpp"
#include "colour.hpp"
#include <array>

using namespace Helpers;

void Texture::allocate() {
    glGenTextures(1, &texture.m_handle);
    texture.create(size.u(), size.v(), GL_RGBA8);
    texture.bind();

#ifdef GPU_DEBUG_INFO
	const auto name = Helpers::format("Surface %dx%d %s from 0x%08X", size.x(), size.y(), PICA::textureFormatToString(format), location);
	OpenGL::setObjectLabel(GL_TEXTURE, texture.handle(), name.c_str());
#endif

    setNewConfig(config);
}

// Set the texture's configuration, which includes min/mag filters, wrapping S/T modes, and so on
void Texture::setNewConfig(u32 cfg) {
    config = cfg;
    // The wrapping mode field is 3 bits instead of 2 bits. The bottom 4 undocumented wrapping modes are taken from Citra.
    static constexpr std::array<OpenGL::WrappingMode, 8> wrappingModes = {
        OpenGL::ClampToEdge, OpenGL::ClampToBorder, OpenGL::Repeat, OpenGL::RepeatMirrored,
        OpenGL::ClampToEdge, OpenGL::ClampToBorder, OpenGL::Repeat, OpenGL::Repeat
    };

    const auto magFilter = (cfg & 0x2) != 0 ? OpenGL::Linear : OpenGL::Nearest;
    const auto minFilter = (cfg & 0x4) != 0 ? OpenGL::Linear : OpenGL::Nearest;
    const auto wrapT = wrappingModes[getBits<8, 3>(cfg)];
    const auto wrapS = wrappingModes[getBits<12, 3>(cfg)];

    texture.setMinFilter(minFilter);
    texture.setMagFilter(magFilter);
    texture.setWrapS(wrapS);
    texture.setWrapT(wrapT);
}

void Texture::free() {
	valid = false;

	if (texture.exists()) {
		texture.free();
	}
}

u64 Texture::sizeInBytes() {
    u64 pixelCount = u64(size.x()) * u64(size.y());

    switch (format) {
        case PICA::TextureFmt::RGBA8: // 4 bytes per pixel
            return pixelCount * 4;

        case PICA::TextureFmt::RGB8: // 3 bytes per pixel
            return pixelCount * 3;

        case PICA::TextureFmt::RGBA5551: // 2 bytes per pixel
        case PICA::TextureFmt::RGB565:
        case PICA::TextureFmt::RGBA4:
        case PICA::TextureFmt::RG8:
        case PICA::TextureFmt::IA8:
            return pixelCount * 2;

        case PICA::TextureFmt::A8: // 1 byte per pixel
        case PICA::TextureFmt::I8:
        case PICA::TextureFmt::IA4:
            return pixelCount;

        case PICA::TextureFmt::I4: // 4 bits per pixel
        case PICA::TextureFmt::A4:
            return pixelCount / 2;

        case PICA::TextureFmt::ETC1: // Compressed formats
        case PICA::TextureFmt::ETC1A4: {
            // Number of 4x4 tiles
            const u64 tileCount = pixelCount / 16;
            // Tiles are 8 bytes each on ETC1 and 16 bytes each on ETC1A4
            const u64 tileSize = format == PICA::TextureFmt::ETC1 ? 8 : 16;
            return tileCount * tileSize;
        }

        default:
            Helpers::panic("[PICA] Attempted to get size of invalid texture type");
        }
}

// u and v are the UVs of the relevant texel
// Texture data is stored interleaved in Morton order, ie in a Z - order curve as shown here
// https://en.wikipedia.org/wiki/Z-order_curve
// Textures are split into 8x8 tiles.This function returns the in - tile offset depending on the u & v of the texel
// The in - tile offset is the sum of 2 offsets, one depending on the value of u % 8 and the other on the value of y % 8
// As documented in this picture https ://en.wikipedia.org/wiki/File:Moser%E2%80%93de_Bruijn_addition.svg
u32 Texture::mortonInterleave(u32 u, u32 v) {
    static constexpr u32 xOffsets[] = { 0, 1, 4, 5, 16, 17, 20, 21 };
    static constexpr u32 yOffsets[] = { 0, 2, 8, 10, 32, 34, 40, 42 };

    return xOffsets[u & 7] + yOffsets[v & 7];
}

// Get the byte offset of texel (u, v) in the texture
u32 Texture::getSwizzledOffset(u32 u, u32 v, u32 width, u32 bytesPerPixel) {
    u32 offset = ((u & ~7) * 8) + ((v & ~7) * width); // Offset of the 8x8 tile the texel belongs to
    offset += mortonInterleave(u, v); // Add the in-tile offset of the texel

    return offset * bytesPerPixel;
}

// Same as the above code except we need to divide by 2 because 4 bits is smaller than a byte
u32 Texture::getSwizzledOffset_4bpp(u32 u, u32 v, u32 width) {
    u32 offset = ((u & ~7) * 8) + ((v & ~7) * width); // Offset of the 8x8 tile the texel belongs to
    offset += mortonInterleave(u, v); // Add the in-tile offset of the texel

    return offset / 2;
}

// Get the texel at position (u, v)
// fmt: format of the texture
// data: texture data of the texture
u32 Texture::decodeTexel(u32 u, u32 v, PICA::TextureFmt fmt, std::span<const u8> data) {
    switch (fmt) {
        case PICA::TextureFmt::RGBA4: {
            u32 offset = getSwizzledOffset(u, v, size.u(), 2);
            u16 texel = u16(data[offset]) | (u16(data[offset + 1]) << 8);

            u8 alpha = Colour::convert4To8Bit(getBits<0, 4, u8>(texel));
            u8 b = Colour::convert4To8Bit(getBits<4, 4, u8>(texel));
            u8 g = Colour::convert4To8Bit(getBits<8, 4, u8>(texel));
            u8 r = Colour::convert4To8Bit(getBits<12, 4, u8>(texel));

            return (alpha << 24) | (b << 16) | (g << 8) | r;
        }

        case PICA::TextureFmt::RGBA5551: {
            const u32 offset = getSwizzledOffset(u, v, size.u(), 2);
            const u16 texel = u16(data[offset]) | (u16(data[offset + 1]) << 8);

            u8 alpha = getBit<0>(texel) ? 0xff : 0;
            u8 b = Colour::convert5To8Bit(getBits<1, 5, u8>(texel));
            u8 g = Colour::convert5To8Bit(getBits<6, 5, u8>(texel));
            u8 r = Colour::convert5To8Bit(getBits<11, 5, u8>(texel));

            return (alpha << 24) | (b << 16) | (g << 8) | r;
        }

        case PICA::TextureFmt::RGB565: {
            const u32 offset = getSwizzledOffset(u, v, size.u(), 2);
            const u16 texel = u16(data[offset]) | (u16(data[offset + 1]) << 8);

            const u8 b = Colour::convert5To8Bit(getBits<0, 5, u8>(texel));
            const u8 g = Colour::convert6To8Bit(getBits<5, 6, u8>(texel));
            const u8 r = Colour::convert5To8Bit(getBits<11, 5, u8>(texel));

            return (0xff << 24) | (b << 16) | (g << 8) | r;
        }

        case PICA::TextureFmt::RG8: {
            u32 offset = getSwizzledOffset(u, v, size.u(), 2);
            constexpr u8 b = 0;
            const u8 g = data[offset];
            const u8 r = data[offset + 1];

            return (0xff << 24) | (b << 16) | (g << 8) | r;
        }

        case PICA::TextureFmt::RGB8: {
            const u32 offset = getSwizzledOffset(u, v, size.u(), 3);
            const u8 b = data[offset];
            const u8 g = data[offset + 1];
            const u8 r = data[offset + 2];

            return (0xff << 24) | (b << 16) | (g << 8) | r;
        }

        case PICA::TextureFmt::RGBA8: {
            const u32 offset = getSwizzledOffset(u, v, size.u(), 4);
            const u8 alpha = data[offset];
            const u8 b = data[offset + 1];
            const u8 g = data[offset + 2];
            const u8 r = data[offset + 3];

            return (alpha << 24) | (b << 16) | (g << 8) | r;
        }

        case PICA::TextureFmt::IA4: {
            const u32 offset = getSwizzledOffset(u, v, size.u(), 1);
            const u8 texel = data[offset];
            const u8 alpha = Colour::convert4To8Bit(texel & 0xf);
            const u8 intensity = Colour::convert4To8Bit(texel >> 4);

            // Intensity formats just copy the intensity value to every colour channel
            return (alpha << 24) | (intensity << 16) | (intensity << 8) | intensity;
        }

        case PICA::TextureFmt::A4: {
            const u32 offset = getSwizzledOffset_4bpp(u, v, size.u());

            // For odd U coordinates, grab the top 4 bits, and the low 4 bits for even coordinates
            u8 alpha = data[offset] >> ((u % 2) ? 4 : 0);
            alpha = Colour::convert4To8Bit(getBits<0, 4>(alpha));

            // A8 sets RGB to 0
            return (alpha << 24) | (0 << 16) | (0 << 8) | 0;
        }

        case PICA::TextureFmt::A8: {
            u32 offset = getSwizzledOffset(u, v, size.u(), 1);
            const u8 alpha = data[offset];

            // A8 sets RGB to 0
            return (alpha << 24) | (0 << 16) | (0 << 8) | 0;
        }

        case PICA::TextureFmt::I4: {
            u32 offset = getSwizzledOffset_4bpp(u, v, size.u());

            // For odd U coordinates, grab the top 4 bits, and the low 4 bits for even coordinates
            u8 intensity = data[offset] >> ((u % 2) ? 4 : 0);
            intensity = Colour::convert4To8Bit(getBits<0, 4>(intensity));

            // Intensity formats just copy the intensity value to every colour channel
            return (0xff << 24) | (intensity << 16) | (intensity << 8) | intensity;
        }

        case PICA::TextureFmt::I8: {
            u32 offset = getSwizzledOffset(u, v, size.u(), 1);
            const u8 intensity = data[offset];

            // Intensity formats just copy the intensity value to every colour channel
            return (0xff << 24) | (intensity << 16) | (intensity << 8) | intensity;
        }

        case PICA::TextureFmt::IA8: {
            u32 offset = getSwizzledOffset(u, v, size.u(), 2);

            // Same as I8 except each pixel gets its own alpha value too
            const u8 alpha = data[offset];
            const u8 intensity = data[offset + 1];
            return (alpha << 24) | (intensity << 16) | (intensity << 8) | intensity;
        }

        case PICA::TextureFmt::ETC1: return getTexelETC(false, u, v, size.u(), data);
        case PICA::TextureFmt::ETC1A4: return getTexelETC(true, u, v, size.u(), data);

        default:
            Helpers::panic("[Texture::DecodeTexel] Unimplemented format = %d", static_cast<int>(fmt));
    }
}

void Texture::decodeTexture(std::span<const u8> data) {
	std::vector<u32> decoded;
	decoded.reserve(u64(size.u()) * u64(size.v()));

	// Decode texels line by line
	for (u32 v = size.v() - 1; s32(v) >= 0; v--) {
		for (u32 u = 0; u < size.u(); u++) {
			u32 colour = decodeTexel(u, v, format, data);
			decoded.push_back(colour);
		}
	}

	texture.bind();
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size.u(), size.v(), GL_RGBA, GL_UNSIGNED_BYTE, decoded.data());
}
