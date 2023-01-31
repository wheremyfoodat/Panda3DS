#include "renderer_gl/textures.hpp"
#include "colour.hpp"

void Texture::allocate() {
    Helpers::panic("Tried to allocate texture");
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
        // Number of 8x8 tiles
        const u64 tileCount = pixelCount / 64;
        // Each 8x8 consists of 4 4x4 tiles, which are 8 bytes each on ETC1 and 16 bytes each on ETC1A4
        const u64 tileSize = 4 * (format == Formats::ETC1 ? 8 : 16);
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

u32 Texture::decodeTexel(u32 u, u32 v, Texture::Formats fmt, void* data) {
    switch (fmt) {
        case Formats::RGBA4: {
            u32 offset = getSwizzledOffset(u, v, size.u(), 2);
            u8* ptr = static_cast<u8*>(data);
            u16 texel = u16(ptr[offset]) | (u16(ptr[offset + 1]) << 8);

            u8 alpha = Colour::convert4To8Bit(texel & 0xf);
            u8 b = Colour::convert4To8Bit((texel >> 4) & 0xf);
            u8 g = Colour::convert4To8Bit((texel >> 8) & 0xf);
            u8 r = Colour::convert4To8Bit((texel >> 12) & 0xf);

            return (r << 24) | (g << 16) | (b << 8) | alpha;
        }

        default:
            Helpers::panic("[Texture::DecodeTexel] Unimplemented format = %d", static_cast<int>(fmt));
    }
}

void Texture::decodeTexture(void* data) {
    std::vector<u32> decoded;
    decoded.reserve(size.u() * size.v());

    // Decode texels line by line
    for (u32 v = 0; v < size.v(); v++) {
        for (u32 u = 0; u < size.u(); u++) {
            u32 colour = decodeTexel(u, v, format, data);
            decoded.push_back(colour);
        }
    }
}