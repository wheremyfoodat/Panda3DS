#include "renderer_mtl/pica_to_mtl.hpp"

#include "renderer_mtl/texture_decoder.hpp"

using namespace Helpers;

namespace PICA {
	MTLPixelFormatInfo mtlPixelFormatInfos[14] = {
		{MTL::PixelFormatRGBA8Unorm, 4, decodeTexelABGR8ToRGBA8},     // RGBA8
		{MTL::PixelFormatRGBA8Unorm, 4, decodeTexelBGR8ToRGBA8},      // RGB8
		{MTL::PixelFormatBGR5A1Unorm, 2, decodeTexelA1BGR5ToBGR5A1},  // RGBA5551
		{MTL::PixelFormatB5G6R5Unorm, 2, decodeTexelB5G6R5ToB5G6R5},  // RGB565
		{MTL::PixelFormatABGR4Unorm, 2, decodeTexelABGR4ToABGR4},     // RGBA4
		{MTL::PixelFormatRG8Unorm,
		 2,
		 decodeTexelAI8ToRG8,
		 true,
		 {
			 .red = MTL::TextureSwizzleRed,
			 .green = MTL::TextureSwizzleRed,
			 .blue = MTL::TextureSwizzleRed,
			 .alpha = MTL::TextureSwizzleGreen,
		 }},                                                 // IA8
		{MTL::PixelFormatRG8Unorm, 2, decodeTexelGR8ToRG8},  // RG8
		{MTL::PixelFormatR8Unorm,
		 1,
		 decodeTexelI8ToR8,
		 true,
		 {.red = MTL::TextureSwizzleRed, .green = MTL::TextureSwizzleRed, .blue = MTL::TextureSwizzleRed, .alpha = MTL::TextureSwizzleOne}},  // I8
		{MTL::PixelFormatA8Unorm, 1, decodeTexelA8ToA8},                                                                                      // A8
		{MTL::PixelFormatABGR4Unorm, 2, decodeTexelAI4ToABGR4},                                                                               // IA4
		{MTL::PixelFormatR8Unorm,
		 1,
		 decodeTexelI4ToR8,
		 true,
		 {.red = MTL::TextureSwizzleRed, .green = MTL::TextureSwizzleRed, .blue = MTL::TextureSwizzleRed, .alpha = MTL::TextureSwizzleOne}},  // I4
		{MTL::PixelFormatA8Unorm, 1, decodeTexelA4ToA8},                                                                                      // A4
		{MTL::PixelFormatRGBA8Unorm, 4, decodeTexelETC1ToRGBA8},                                                                              // ETC1
		{MTL::PixelFormatRGBA8Unorm, 4, decodeTexelETC1A4ToRGBA8},  // ETC1A4
	};

	void checkForMTLPixelFormatSupport(MTL::Device* device) {
#ifndef PANDA3DS_IOS_SIMULATOR
		const bool supportsApple1 = device->supportsFamily(MTL::GPUFamilyApple1);
#else
		// iOS simulator claims to support Apple1, yet doesn't support a bunch of texture formats from it...
		const bool supportsApple1 = false;
#endif
		if (!supportsApple1) {
			mtlPixelFormatInfos[2] = {MTL::PixelFormatRGBA8Unorm, 4, decodeTexelA1BGR5ToRGBA8};
			mtlPixelFormatInfos[3] = {MTL::PixelFormatRGBA8Unorm, 4, decodeTexelB5G6R5ToRGBA8};
			mtlPixelFormatInfos[4] = {MTL::PixelFormatRGBA8Unorm, 4, decodeTexelABGR4ToRGBA8};

			mtlPixelFormatInfos[9] = {
				MTL::PixelFormatRG8Unorm,
				2,
				decodeTexelAI4ToRG8,
				true,
				{
					.red = MTL::TextureSwizzleRed,
					.green = MTL::TextureSwizzleRed,
					.blue = MTL::TextureSwizzleRed,
					.alpha = MTL::TextureSwizzleGreen,
				}
			};
		}
	}
}  // namespace PICA
