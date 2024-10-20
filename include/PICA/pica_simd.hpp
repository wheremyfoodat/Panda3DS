#pragma once
#include <algorithm>
#include <limits>
#include <utility>

#include "helpers.hpp"

#if defined(_M_AMD64) || defined(__x86_64__)
#define PICA_SIMD_X64
#elif defined(_M_ARM64) || defined(__aarch64__)
#define PICA_SIMD_ARM64
#include <arm_neon.h>
#endif

// Optimized functions for analyzing PICA index buffers (Finding minimum and maximum index values inside them)
namespace PICA::IndexBuffer {
	// Non-SIMD, portable algorithm
	template <bool useShortIndices>
	std::pair<u16, u16> analyzePortable(u8* indexBuffer, u32 vertexCount) {
		u16 minimumIndex = std::numeric_limits<u16>::max();
		u16 maximumIndex = 0;

		// Calculate the minimum and maximum indices used in the index buffer, so we'll only upload them
		if constexpr (useShortIndices) {
			u16* indexBuffer16 = reinterpret_cast<u16*>(indexBuffer);

			for (int i = 0; i < vertexCount; i++) {
				u16 index = indexBuffer16[i];
				minimumIndex = std::min(minimumIndex, index);
				maximumIndex = std::max(maximumIndex, index);
			}
		} else {
			for (int i = 0; i < vertexCount; i++) {
				u16 index = u16(indexBuffer[i]);
				minimumIndex = std::min(minimumIndex, index);
				maximumIndex = std::max(maximumIndex, index);
			}
		}

		return {minimumIndex, maximumIndex};
	}

#ifdef PICA_SIMD_ARM64
	template <bool useShortIndices>
	std::pair<u16, u16> analyzeNEON(u8* indexBuffer, u32 vertexCount) {
		// We process 16 bytes per iteration, which is 8 vertices if we're using u16 indices or 16 vertices if we're using u8 indices
		constexpr u32 vertsPerLoop = (useShortIndices) ? 8 : 16;

		if (vertexCount < vertsPerLoop) {
			return analyzePortable<useShortIndices>(indexBuffer, vertexCount);
		}

		u16 minimumIndex, maximumIndex;

		if constexpr (useShortIndices) {
			// 16-bit indices
			uint16x8_t minima = vdupq_n_u16(0xffff);
			uint16x8_t maxima = vdupq_n_u16(0);

			while (vertexCount >= vertsPerLoop) {
				const uint16x8_t data = vld1q_u16(reinterpret_cast<u16*>(indexBuffer));
				minima = vminq_u16(data, minima);
				maxima = vmaxq_u16(data, maxima);

				indexBuffer += 16;
				vertexCount -= vertsPerLoop;
			}

			// Do horizontal min/max operations to get the actual minimum and maximum from all the vertices we processed with SIMD
			// We want to gather the actual minimum and maximum in the line bottom lane of the minima/maxima vectors
			// uint16x4_t foldedMinima1 = vmin_u16(vget_high_u16(minima), vget_low_u16(minima));
			// uint16x4_t foldedMaxima1 = vmax_u16(vget_high_u16(maxima), vget_low_u16(maxima));

			uint16x8_t foldedMinima1 = vpminq_u16(minima, minima);
			uint16x8_t foldedMinima2 = vpminq_u16(foldedMinima1, foldedMinima1);
			uint16x8_t foldedMinima3 = vpminq_u16(foldedMinima2, foldedMinima2);

			uint16x8_t foldedMaxima1 = vpmaxq_u16(maxima, maxima);
			uint16x8_t foldedMaxima2 = vpmaxq_u16(foldedMaxima1, foldedMaxima1);
			uint16x8_t foldedMaxima3 = vpmaxq_u16(foldedMaxima2, foldedMaxima2);

			minimumIndex = vgetq_lane_u16(foldedMinima3, 0);
			maximumIndex = vgetq_lane_u16(foldedMaxima3, 0);
		} else {
			// 8-bit indices
			uint8x16_t minima = vdupq_n_u8(0xff);
			uint8x16_t maxima = vdupq_n_u8(0);

			while (vertexCount >= vertsPerLoop) {
				uint8x16_t data = vld1q_u8(indexBuffer);
				minima = vminq_u8(data, minima);
				maxima = vmaxq_u8(data, maxima);

				indexBuffer += 16;
				vertexCount -= vertsPerLoop;
			}

			// Do a similar horizontal min/max as in the u16 case, except now we're working uint8x16 instead of uint16x4 so we need 4 folds
			uint8x16_t foldedMinima1 = vpminq_u8(minima, minima);
			uint8x16_t foldedMinima2 = vpminq_u8(foldedMinima1, foldedMinima1);
			uint8x16_t foldedMinima3 = vpminq_u8(foldedMinima2, foldedMinima2);
			uint8x16_t foldedMinima4 = vpminq_u8(foldedMinima3, foldedMinima3);

			uint8x16_t foldedMaxima1 = vpmaxq_u8(maxima, maxima);
			uint8x16_t foldedMaxima2 = vpmaxq_u8(foldedMaxima1, foldedMaxima1);
			uint8x16_t foldedMaxima3 = vpmaxq_u8(foldedMaxima2, foldedMaxima2);
			uint8x16_t foldedMaxima4 = vpmaxq_u8(foldedMaxima3, foldedMaxima3);

			minimumIndex = u16(vgetq_lane_u8(foldedMinima4, 0));
			maximumIndex = u16(vgetq_lane_u8(foldedMaxima4, 0));
		}

		// If any indices could not be processed cause the buffer size is not 16-byte aligned, process them the naive way
		// Calculate the minimum and maximum indices used in the index buffer, so we'll only upload them
		while (vertexCount > 0) {
			if constexpr (useShortIndices) {
				u16 index = *reinterpret_cast<u16*>(indexBuffer);
				minimumIndex = std::min(minimumIndex, index);
				maximumIndex = std::max(maximumIndex, index);
				indexBuffer += 2;
			} else {
				u16 index = u16(*indexBuffer++);
				minimumIndex = std::min(minimumIndex, index);
				maximumIndex = std::max(maximumIndex, index);
			}

			vertexCount -= 1;
		}

		return {minimumIndex, maximumIndex};
	}
#endif

	// Analyzes a PICA index buffer to get the minimum and maximum indices in the buffer, and returns them in a pair in the form [min, max]
	// Takes a template parameter to decide whether the indices in the buffer are in u8 or u16 format.
	template <bool useShortIndices>
	std::pair<u16, u16> analyze(u8* indexBuffer, u32 vertexCount) {
#ifdef PICA_SIMD_ARM64
		return analyzeNEON<useShortIndices>(indexBuffer, vertexCount);
#else
		return analyzePortable<useShortIndices>(indexBuffer, vertexCount);
#endif
	}
}  // namespace PICA::IndexBuffer
