#include "renderer_mtl/renderer_mtl.hpp"

namespace Metal {

constexpr u32 LAYER_COUNT = 1024;

LutTexture::LutTexture(MTL::Device* device, MTL::TextureType type, MTL::PixelFormat pixelFormat, u32 width, u32 height, const char* name) {
    MTL::TextureDescriptor* desc = MTL::TextureDescriptor::alloc()->init();
	desc->setTextureType(type);
	desc->setPixelFormat(pixelFormat);
	desc->setWidth(width);
	desc->setHeight(height);
	desc->setArrayLength(LAYER_COUNT);
	desc->setUsage(MTL::TextureUsageShaderRead/* | MTL::TextureUsageShaderWrite*/);
	desc->setStorageMode(MTL::StorageModeShared);

	texture = device->newTexture(desc);
	texture->setLabel(toNSString(name));
	desc->release();
}

LutTexture::~LutTexture() {
    texture->release();
}

u32 LutTexture::getNextIndex() {
    currentIndex = (currentIndex + 1) % LAYER_COUNT;

    return currentIndex;
}

} // namespace Metal
