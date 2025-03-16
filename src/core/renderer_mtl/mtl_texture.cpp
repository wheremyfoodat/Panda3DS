#include "renderer_mtl/mtl_texture.hpp"

#include <fmt/format.h>

#include <array>
#include <memory>

#include "colour.hpp"
#include "renderer_mtl/objc_helper.hpp"

using namespace Helpers;

namespace Metal {
	void Texture::allocate() {
		formatInfo = PICA::getMTLPixelFormatInfo(format);

		MTL::TextureDescriptor* descriptor = MTL::TextureDescriptor::alloc()->init();
		descriptor->setTextureType(MTL::TextureType2D);
		descriptor->setPixelFormat(formatInfo.pixelFormat);
		descriptor->setWidth(size.u());
		descriptor->setHeight(size.v());
		descriptor->setUsage(MTL::TextureUsageShaderRead);
		descriptor->setStorageMode(MTL::StorageModeShared);  // TODO: use private + staging buffers?
		texture = device->newTexture(descriptor);
		texture->setLabel(toNSString(fmt::format("Base texture {} {}x{}", std::string(PICA::textureFormatToString(format)), size.u(), size.v())));
		descriptor->release();

		if (formatInfo.needsSwizzle) {
		    base = texture;
		    texture = base->newTextureView(formatInfo.pixelFormat, MTL::TextureType2D, NS::Range(0, 1), NS::Range(0, 1), formatInfo.swizzle);
		}

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

		if (base) {
			base->release();
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

	void Texture::decodeTexture(std::span<const u8> data) {
		std::unique_ptr<u8[]> decodedData(new u8[u64(size.u()) * u64(size.v()) * formatInfo.bytesPerTexel]);
		// This pointer will be incremented by our texture decoders
		u8* decodePtr = decodedData.get();

		// Decode texels line by line
		for (u32 v = 0; v < size.v(); v++) {
			for (u32 u = 0; u < size.u(); u++) {
				formatInfo.decoder(size, u, v, data, decodePtr);
				decodePtr += formatInfo.bytesPerTexel;
			}
		}

		texture->replaceRegion(MTL::Region(0, 0, size.u(), size.v()), 0, 0, decodedData.get(), formatInfo.bytesPerTexel * size.u(), 0);
	}
}  // namespace Metal
