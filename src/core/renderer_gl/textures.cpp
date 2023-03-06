#include "renderer_gl/textures.hpp"
#include "colour.hpp"

void Texture::allocate() {
    glGenTextures(1, &texture.m_handle);
    texture.create(size.u(), size.v(), GL_RGBA8);
    texture.bind();

    texture.setMinFilter(OpenGL::Nearest);
    texture.setMagFilter(OpenGL::Nearest);
    texture.setWrapS(OpenGL::Repeat);
    texture.setWrapT(OpenGL::Repeat);
}

void Texture::free() {
    valid = false;

    if (texture.exists())
        Helpers::panic("Make this texture free itself");
}

u64 Texture::sizeInBytes() {
    u64 pixelCount = u64(size.x()) * u64(size.y());

    switch (format) {
        case Formats::RGBA8: // 4 bytes per pixel
            return pixelCount * 4;

        case Formats::RGB8: // 3 bytes per pixel
            return pixelCount * 3;

        case Formats::RGBA5551: // 2 bytes per pixel
        case Formats::RGB565:
        case Formats::RGBA4:
        case Formats::RG8:
        case Formats::IA8:
            return pixelCount * 2;

        case Formats::A8: // 1 byte per pixel
        case Formats::I8:
        case Formats::IA4:
            return pixelCount;

        case Formats::I4: // 4 bits per pixel
        case Formats::A4:
            return pixelCount / 2;

        case Formats::ETC1: // Compressed formats
        case Formats::ETC1A4: {
            // Number of 4x4 tiles
            const u64 tileCount = pixelCount / 16;
            // Tiles are 8 bytes each on ETC1 and 16 bytes each on ETC1A4
            const u64 tileSize = format == Formats::ETC1 ? 8 : 16;
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
u32 Texture::decodeTexel(u32 u, u32 v, Texture::Formats fmt, const void* data) {
    switch (fmt) {
        case Formats::RGBA4: {
            u32 offset = getSwizzledOffset(u, v, size.u(), 2);
            auto ptr = static_cast<const u8*>(data);
            u16 texel = u16(ptr[offset]) | (u16(ptr[offset + 1]) << 8);

            u8 alpha = Colour::convert4To8Bit(texel & 0xf);
            u8 b = Colour::convert4To8Bit((texel >> 4) & 0xf);
            u8 g = Colour::convert4To8Bit((texel >> 8) & 0xf);
            u8 r = Colour::convert4To8Bit((texel >> 12) & 0xf);

            return (alpha << 24) | (b << 16) | (g << 8) | r;
        }

        case Formats::RGBA5551: {
            u32 offset = getSwizzledOffset(u, v, size.u(), 2);
            auto ptr = static_cast<const u8*>(data);
            u16 texel = u16(ptr[offset]) | (u16(ptr[offset + 1]) << 8);

            u8 alpha = (texel & 1) ? 0xff : 0;
            u8 b = Colour::convert5To8Bit((texel >> 1) & 0x1f);
            u8 g = Colour::convert5To8Bit((texel >> 6) & 0x1f);
            u8 r = Colour::convert5To8Bit((texel >> 11) & 0x1f);

            return (alpha << 24) | (b << 16) | (g << 8) | r;
        }

        case Formats::RGB565: {
            u32 offset = getSwizzledOffset(u, v, size.u(), 2);
            auto ptr = static_cast<const u8*>(data);
            u16 texel = u16(ptr[offset]) | (u16(ptr[offset + 1]) << 8);

            u8 b = Colour::convert5To8Bit(texel & 0x1f);
            u8 g = Colour::convert6To8Bit((texel >> 5) & 0x3f);
            u8 r = Colour::convert5To8Bit((texel >> 11) & 0x1f);

            return (0xff << 24) | (b << 16) | (g << 8) | r;
        }

        case Formats::RGBA8: {
            u32 offset = getSwizzledOffset(u, v, size.u(), 4);
            auto ptr = static_cast<const u8*>(data);

            u8 alpha = ptr[offset];
            u8 b = ptr[offset + 1];
            u8 g = ptr[offset + 2];
            u8 r = ptr[offset + 3];

            return (alpha << 24) | (b << 16) | (g << 8) | r;
        }

        case Formats::IA4: {
            u32 offset = getSwizzledOffset(u, v, size.u(), 1);
            auto ptr = static_cast<const u8*>(data);

            const u8 texel = ptr[offset];
            const u8 alpha = Colour::convert4To8Bit(texel & 0xf);
            const u8 intensity = Colour::convert4To8Bit(texel >> 4);

            // Intensity formats just copy the intensity value to every colour channel
            return (alpha << 24) | (intensity << 16) | (intensity << 8) | intensity;
        }

        case Formats::A4: {
            u32 offset = getSwizzledOffset_4bpp(u, v, size.u());
            auto ptr = static_cast<const u8*>(data);

            // For odd U coordinates, grab the top 4 bits, and the low 4 bits for even coordinates
            u8 alpha = ptr[offset] >> ((u % 2) ? 4 : 0);
            alpha = Colour::convert4To8Bit(alpha & 0xf);

            // A8 sets RGB to 0
            return (alpha << 24) | (0 << 16) | (0 << 8) | 0;
        }

        case Formats::A8: {
            u32 offset = getSwizzledOffset(u, v, size.u(), 1);
            auto ptr = static_cast<const u8*>(data);
            const u8 alpha = ptr[offset];

            // A8 sets RGB to 0
            return (alpha << 24) | (0 << 16) | (0 << 8) | 0;
        }

        case Formats::I8: {
            u32 offset = getSwizzledOffset(u, v, size.u(), 1);
            auto ptr = static_cast<const u8*>(data);
            const u8 intensity = ptr[offset];

            // Intensity formats just copy the intensity value to every colour channel
            return (0xff << 24) | (intensity << 16) | (intensity << 8) | intensity;
        }

        case Formats::IA8: {
            u32 offset = getSwizzledOffset(u, v, size.u(), 2);
            auto ptr = static_cast<const u8*>(data);

            // Same as I8 except each pixel gets its own alpha value too
            const u8 alpha = ptr[offset];
            const u8 intensity = ptr[offset + 1];
            return (alpha << 24) | (intensity << 16) | (intensity << 8) | intensity;
        }

        case Formats::ETC1: return getTexelETC(false, u, v, size.u(), data);
        case Formats::ETC1A4: return getTexelETC(true, u, v, size.u(), data);

        default:
            Helpers::panic("[Texture::DecodeTexel] Unimplemented format = %d", static_cast<int>(fmt));
    }
}

void Texture::decodeTexture(const void* data) {
    std::vector<u32> decoded;
    decoded.reserve(u64(size.u()) * u64(size.v()));

    // Decode texels line by line
    for (u32 v = 0; v < size.v(); v++) {
        for (u32 u = 0; u < size.u(); u++) {
            u32 colour = decodeTexel(u, v, format, data);
            decoded.push_back(colour);
        }
    }

    texture.bind();
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size.u(), size.v(), GL_RGBA, GL_UNSIGNED_BYTE, decoded.data());
}