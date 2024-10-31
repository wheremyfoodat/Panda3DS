#pragma once

#include <Metal/Metal.hpp>

namespace Metal {

class LutTexture {
public:
    LutTexture(MTL::Device* device, MTL::TextureType type, MTL::PixelFormat pixelFormat, u32 width, u32 height, const char* name);
    ~LutTexture();

    u32 getNextIndex();

    // Getters
    MTL::Texture* getTexture() { return texture; }

    u32 getCurrentIndex() { return currentIndex; }

private:
    MTL::Texture* texture;

    u32 currentIndex = 0;
};

} // namespace Metal
