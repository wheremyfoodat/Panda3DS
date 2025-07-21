#include "renderer_mtl/texture_decoder.hpp"

#include <array>
#include <string>

#include "colour.hpp"
#include "math_util.hpp"

using namespace Helpers;

// u and v are the UVs of the relevant texel
// Texture data is stored interleaved in Morton order, ie in a Z - order curve as shown here
// https://en.wikipedia.org/wiki/Z-order_curve
// Textures are split into 8x8 tiles.This function returns the in - tile offset depending on the u & v of the texel
// The in - tile offset is the sum of 2 offsets, one depending on the value of u % 8 and the other on the value of y % 8
// As documented in this picture https ://en.wikipedia.org/wiki/File:Moser%E2%80%93de_Bruijn_addition.svg
u32 mortonInterleave(u32 u, u32 v) {
	static constexpr u32 xOffsets[] = {0, 1, 4, 5, 16, 17, 20, 21};
	static constexpr u32 yOffsets[] = {0, 2, 8, 10, 32, 34, 40, 42};

	return xOffsets[u & 7] + yOffsets[v & 7];
}

// Get the byte offset of texel (u, v) in the texture
u32 getSwizzledOffset(u32 u, u32 v, u32 width, u32 bytesPerPixel) {
	u32 offset = ((u & ~7) * 8) + ((v & ~7) * width);  // Offset of the 8x8 tile the texel belongs to
	offset += mortonInterleave(u, v);                  // Add the in-tile offset of the texel

	return offset * bytesPerPixel;
}

// Same as the above code except we need to divide by 2 because 4 bits is smaller than a byte
u32 getSwizzledOffset_4bpp(u32 u, u32 v, u32 width) {
	u32 offset = ((u & ~7) * 8) + ((v & ~7) * width);  // Offset of the 8x8 tile the texel belongs to
	offset += mortonInterleave(u, v);                  // Add the in-tile offset of the texel

	return offset / 2;
}

void decodeTexelABGR8ToRGBA8(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData) {
	const u32 offset = getSwizzledOffset(u, v, size.u(), 4);
	const u8 alpha = inData[offset];
	const u8 b = inData[offset + 1];
	const u8 g = inData[offset + 2];
	const u8 r = inData[offset + 3];

	*outData++ = r;
	*outData++ = g;
	*outData++ = b;
	*outData++ = alpha;
}

void decodeTexelBGR8ToRGBA8(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData) {
	const u32 offset = getSwizzledOffset(u, v, size.u(), 3);
	const u8 b = inData[offset];
	const u8 g = inData[offset + 1];
	const u8 r = inData[offset + 2];

	*outData++ = r;
	*outData++ = g;
	*outData++ = b;
	*outData++ = 0xff;
}

void decodeTexelA1BGR5ToBGR5A1(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData) {
	const u32 offset = getSwizzledOffset(u, v, size.u(), 2);
	const u16 texel = u16(inData[offset]) | (u16(inData[offset + 1]) << 8);

	u8 alpha = getBit<0>(texel);
	u8 b = getBits<1, 5, u8>(texel);
	u8 g = getBits<6, 5, u8>(texel);
	u8 r = getBits<11, 5, u8>(texel);

	u16 outTexel = (alpha << 15) | (r << 10) | (g << 5) | b;
	*outData++ = outTexel & 0xff;
	*outData++ = (outTexel >> 8) & 0xff;
}

void decodeTexelA1BGR5ToRGBA8(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData) {
	const u32 offset = getSwizzledOffset(u, v, size.u(), 2);
	const u16 texel = u16(inData[offset]) | (u16(inData[offset + 1]) << 8);

	u8 alpha = getBit<0>(texel) ? 0xff : 0;
	u8 b = Colour::convert5To8Bit(getBits<1, 5, u8>(texel));
	u8 g = Colour::convert5To8Bit(getBits<6, 5, u8>(texel));
	u8 r = Colour::convert5To8Bit(getBits<11, 5, u8>(texel));

	*outData++ = r;
	*outData++ = g;
	*outData++ = b;
	*outData++ = alpha;
}

void decodeTexelB5G6R5ToB5G6R5(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData) {
	const u32 offset = getSwizzledOffset(u, v, size.u(), 2);
	const u16 texel = u16(inData[offset]) | (u16(inData[offset + 1]) << 8);

	*outData++ = texel & 0xff;
	*outData++ = (texel >> 8) & 0xff;
}

void decodeTexelB5G6R5ToRGBA8(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData) {
	const u32 offset = getSwizzledOffset(u, v, size.u(), 2);
	const u16 texel = u16(inData[offset]) | (u16(inData[offset + 1]) << 8);

	const u8 b = Colour::convert5To8Bit(getBits<0, 5, u8>(texel));
	const u8 g = Colour::convert6To8Bit(getBits<5, 6, u8>(texel));
	const u8 r = Colour::convert5To8Bit(getBits<11, 5, u8>(texel));

	*outData++ = r;
	*outData++ = g;
	*outData++ = b;
	*outData++ = 0xff;
}

void decodeTexelABGR4ToABGR4(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData) {
	u32 offset = getSwizzledOffset(u, v, size.u(), 2);
	u16 texel = u16(inData[offset]) | (u16(inData[offset + 1]) << 8);

	u8 alpha = getBits<0, 4, u8>(texel);
	u8 b = getBits<4, 4, u8>(texel);
	u8 g = getBits<8, 4, u8>(texel);
	u8 r = getBits<12, 4, u8>(texel);

	*outData++ = (b << 4) | alpha;
	*outData++ = (r << 4) | g;
}

void decodeTexelABGR4ToRGBA8(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData) {
	u32 offset = getSwizzledOffset(u, v, size.u(), 2);
	u16 texel = u16(inData[offset]) | (u16(inData[offset + 1]) << 8);

	u8 alpha = Colour::convert4To8Bit(getBits<0, 4, u8>(texel));
	u8 b = Colour::convert4To8Bit(getBits<4, 4, u8>(texel));
	u8 g = Colour::convert4To8Bit(getBits<8, 4, u8>(texel));
	u8 r = Colour::convert4To8Bit(getBits<12, 4, u8>(texel));

	*outData++ = r;
	*outData++ = g;
	*outData++ = b;
	*outData++ = alpha;
}

void decodeTexelAI8ToRG8(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData) {
	u32 offset = getSwizzledOffset(u, v, size.u(), 2);

	// Same as I8 except each pixel gets its own alpha value too
	const u8 alpha = inData[offset];
	const u8 intensity = inData[offset + 1];

	*outData++ = intensity;
	*outData++ = alpha;
}

void decodeTexelGR8ToRG8(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData) {
	u32 offset = getSwizzledOffset(u, v, size.u(), 2);
	constexpr u8 b = 0;
	const u8 g = inData[offset];
	const u8 r = inData[offset + 1];

	*outData++ = r;
	*outData++ = g;
}

void decodeTexelI8ToR8(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData) {
	u32 offset = getSwizzledOffset(u, v, size.u(), 1);
	const u8 intensity = inData[offset];

	*outData++ = intensity;
}

void decodeTexelA8ToA8(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData) {
	u32 offset = getSwizzledOffset(u, v, size.u(), 1);
	const u8 alpha = inData[offset];

	*outData++ = alpha;
}

void decodeTexelAI4ToABGR4(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData) {
	const u32 offset = getSwizzledOffset(u, v, size.u(), 1);
	const u8 texel = inData[offset];
	const u8 alpha = texel & 0xf;
	const u8 intensity = texel >> 4;

	*outData++ = (intensity << 4) | intensity;
	*outData++ = (alpha << 4) | intensity;
}

void decodeTexelAI4ToRG8(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData) {
	const u32 offset = getSwizzledOffset(u, v, size.u(), 1);
	const u8 texel = inData[offset];
	const u8 alpha = Colour::convert4To8Bit(texel & 0xf);
	const u8 intensity = Colour::convert4To8Bit(texel >> 4);

	*outData++ = intensity;
	*outData++ = alpha;
}

void decodeTexelI4ToR8(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData) {
	u32 offset = getSwizzledOffset_4bpp(u, v, size.u());

	// For odd U coordinates, grab the top 4 bits, and the low 4 bits for even coordinates
	u8 intensity = inData[offset] >> ((u % 2) ? 4 : 0);
	intensity = Colour::convert4To8Bit(getBits<0, 4>(intensity));

	*outData++ = intensity;
}

void decodeTexelA4ToA8(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData) {
	const u32 offset = getSwizzledOffset_4bpp(u, v, size.u());

	// For odd U coordinates, grab the top 4 bits, and the low 4 bits for even coordinates
	u8 alpha = inData[offset] >> ((u % 2) ? 4 : 0);
	alpha = Colour::convert4To8Bit(getBits<0, 4>(alpha));

	*outData++ = alpha;
}

static constexpr u32 signExtend3To32(u32 val) { return (u32)(s32(val) << 29 >> 29); }

void decodeETC(u32 u, u32 v, u64 colourData, u32 alpha, u8* outData) {
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

	*outData++ = r;
	*outData++ = g;
	*outData++ = b;
	*outData++ = alpha;
}

template <bool hasAlpha>
void getTexelETC(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData) {
	// Pixel offset of the 8x8 tile based on u, v and the width of the texture
	u32 offs = ((u & ~7) * 8) + ((v & ~7) * size.u());
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
	const u64* ptr = reinterpret_cast<const u64*>(inData.data() + offs);  // Cast to u64*

	if (hasAlpha) {
		// First 64 bits of the 4x4 subtile are alpha data
		const u64 alphaData = *ptr++;
		alpha = Colour::convert4To8Bit((alphaData >> (4 * (u * 4 + v))) & 0xf);
	} else {
		alpha = 0xff;  // ETC1 without alpha uses ff for every pixel
	}

	// Next 64 bits of the subtile are colour data
	u64 colourData = *ptr;

	decodeETC(u, v, colourData, alpha, outData);
}

void decodeTexelETC1ToRGBA8(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData) {
	getTexelETC<false>(size, u, v, inData, outData);
}

void decodeTexelETC1A4ToRGBA8(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData) {
	getTexelETC<true>(size, u, v, inData, outData);
}
