#include <algorithm>

#include "colour.hpp"
#include "renderer_mtl/mtl_texture.hpp"
#include "renderer_mtl/renderer_mtl.hpp"


using namespace Helpers;

namespace Metal {
	static constexpr u32 signExtend3To32(u32 val) {
        return (u32)(s32(val) << 29 >> 29);
    }

	u32 Texture::getTexelETC(bool hasAlpha, u32 u, u32 v, u32 width, std::span<const u8> data) {
		// Pixel offset of the 8x8 tile based on u, v and the width of the texture
		u32 offs = ((u & ~7) * 8) + ((v & ~7) * width);
		if (!hasAlpha) {
			offs >>= 1;
		}

		// In-tile offsets for u/v
		u &= 7;
		v &= 7;

		// ETC1(A4) also subdivide the 8x8 tile to 4 4x4 tiles
		// Each tile is 8 bytes for ETC1, but since ETC1A4 has 4 alpha bits per pixel, that becomes 16 bytes
		const u32 subTileSize = hasAlpha ? 16 : 8;
		const u32 subTileIndex = (u / 4) + 2 * (v / 4);  // Which of the 4 subtiles is this texel in?

		// In-subtile offsets for u/v
		u &= 3;
		v &= 3;
		offs += subTileSize * subTileIndex;

		u32 alpha;
		const u64* ptr = reinterpret_cast<const u64*>(data.data() + offs);  // Cast to u64*

		if (hasAlpha) {
			// First 64 bits of the 4x4 subtile are alpha data
			const u64 alphaData = *ptr++;
			alpha = Colour::convert4To8Bit((alphaData >> (4 * (u * 4 + v))) & 0xf);
		} else {
			alpha = 0xff;  // ETC1 without alpha uses ff for every pixel
		}

		// Next 64 bits of the subtile are colour data
		u64 colourData = *ptr;
		return decodeETC(alpha, u, v, colourData);
	}

	u32 Texture::decodeETC(u32 alpha, u32 u, u32 v, u64 colourData) {
		static constexpr u32 modifiers[8][2] = {
			{2, 8}, {5, 17}, {9, 29}, {13, 42}, {18, 60}, {24, 80}, {33, 106}, {47, 183},
		};

		// Parse colour data for 4x4 block
		const u32 subindices = getBits<0, 16, u32>(colourData);
		const u32 negationFlags = getBits<16, 16, u32>(colourData);
		const bool flip = getBit<32>(colourData);
		const bool diffMode = getBit<33>(colourData);

		// Note: index1 is indeed stored on the higher bits, with index2 in the lower bits
		const u32 tableIndex1 = getBits<37, 3, u32>(colourData);
		const u32 tableIndex2 = getBits<34, 3, u32>(colourData);
		const u32 texelIndex = u * 4 + v;  // Index of the texel in the block

		if (flip) std::swap(u, v);

		s32 r, g, b;
		if (diffMode) {
			r = getBits<59, 5, s32>(colourData);
			g = getBits<51, 5, s32>(colourData);
			b = getBits<43, 5, s32>(colourData);

			if (u >= 2) {
				r += signExtend3To32(getBits<56, 3, u32>(colourData));
				g += signExtend3To32(getBits<48, 3, u32>(colourData));
				b += signExtend3To32(getBits<40, 3, u32>(colourData));
			}

			// Expand from 5 to 8 bits per channel
			r = Colour::convert5To8Bit(r);
			g = Colour::convert5To8Bit(g);
			b = Colour::convert5To8Bit(b);
		} else {
			if (u < 2) {
				r = getBits<60, 4, s32>(colourData);
				g = getBits<52, 4, s32>(colourData);
				b = getBits<44, 4, s32>(colourData);
			} else {
				r = getBits<56, 4, s32>(colourData);
				g = getBits<48, 4, s32>(colourData);
				b = getBits<40, 4, s32>(colourData);
			}

			// Expand from 4 to 8 bits per channel
			r = Colour::convert4To8Bit(r);
			g = Colour::convert4To8Bit(g);
			b = Colour::convert4To8Bit(b);
		}

		const u32 index = (u < 2) ? tableIndex1 : tableIndex2;
		s32 modifier = modifiers[index][(subindices >> texelIndex) & 1];

		if (((negationFlags >> texelIndex) & 1) != 0) {
			modifier = -modifier;
		}

		r = std::clamp(r + modifier, 0, 255);
		g = std::clamp(g + modifier, 0, 255);
		b = std::clamp(b + modifier, 0, 255);

		return (alpha << 24) | (u32(b) << 16) | (u32(g) << 8) | u32(r);
	}
}  // namespace Metal
