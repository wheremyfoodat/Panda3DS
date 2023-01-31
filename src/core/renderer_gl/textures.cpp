#include "renderer_gl/textures.hpp"

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