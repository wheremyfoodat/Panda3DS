#pragma once
#include <algorithm>
#include <limits>
#include <utility>

#include "helpers.hpp"

#if defined(_M_AMD64) || defined(__x86_64__)
#define PICA_SIMD_X64
#include <immintrin.h>
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

			for (u32 i = 0; i < vertexCount; i++) {
				u16 index = indexBuffer16[i];
				minimumIndex = std::min(minimumIndex, index);
				maximumIndex = std::max(maximumIndex, index);
			}
		} else {
			for (u32 i = 0; i < vertexCount; i++) {
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

#if defined(PICA_SIMD_X64) && (defined(__SSE4_1__) || defined(__AVX__))
	template <bool useShortIndices>
	std::pair<u16, u16> analyzeSSE4_1(u8* indexBuffer, u32 vertexCount) {
		// We process 16 bytes per iteration, which is 8 vertices if we're using u16
		// indices or 16 vertices if we're using u8 indices
		constexpr u32 vertsPerLoop = (useShortIndices) ? 8 : 16;

		if (vertexCount < vertsPerLoop) {
			return analyzePortable<useShortIndices>(indexBuffer, vertexCount);
		}

		u16 minimumIndex, maximumIndex;

		if constexpr (useShortIndices) {
			// Calculate the horizontal minimum/maximum value across an SSE vector of 16-bit unsigned integers.
			// Based on https://stackoverflow.com/a/22259607
			auto horizontalMin16 = [](__m128i vector) -> u16 { return u16(_mm_cvtsi128_si32(_mm_minpos_epu16(vector))); };

			auto horizontalMax16 = [](__m128i vector) -> u16 {
				// We have an instruction to compute horizontal minimum but not maximum, so we use it.
				// To use it, we have to subtract each value from 0xFFFF (which we do with an xor), then execute a horizontal minimum
				__m128i flipped = _mm_xor_si128(vector, _mm_set_epi32(0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu));
				u16 min = u16(_mm_cvtsi128_si32(_mm_minpos_epu16(flipped)));
				return u16(min ^ 0xffff);
			};

			// 16-bit indices
			// Initialize the minima vector to all FFs (So 0xFFFF for each 16-bit lane)
			// And the maxima vector to all 0s (0 for each 16-bit lane)
			__m128i minima = _mm_set_epi32(0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu);
			__m128i maxima = _mm_set_epi32(0, 0, 0, 0);

			while (vertexCount >= vertsPerLoop) {
				const __m128i data = _mm_loadu_si128(reinterpret_cast<const __m128i*>(indexBuffer));
				minima = _mm_min_epu16(data, minima);
				maxima = _mm_max_epu16(data, maxima);

				indexBuffer += 16;
				vertexCount -= vertsPerLoop;
			}

			minimumIndex = u16(horizontalMin16(minima));
			maximumIndex = u16(horizontalMax16(maxima));
		} else {
			// Calculate the horizontal minimum/maximum value across an SSE vector of 8-bit unsigned integers.
			// Based on https://stackoverflow.com/a/22259607
			auto horizontalMin8 = [](__m128i vector) -> u8 {
				vector = _mm_min_epu8(vector, _mm_shuffle_epi32(vector, _MM_SHUFFLE(3, 2, 3, 2)));
				vector = _mm_min_epu8(vector, _mm_shuffle_epi32(vector, _MM_SHUFFLE(1, 1, 1, 1)));
				vector = _mm_min_epu8(vector, _mm_shufflelo_epi16(vector, _MM_SHUFFLE(1, 1, 1, 1)));
				vector = _mm_min_epu8(vector, _mm_srli_epi16(vector, 8));
				return u8(_mm_cvtsi128_si32(vector));
			};

			auto horizontalMax8 = [](__m128i vector) -> u8 {
				vector = _mm_max_epu8(vector, _mm_shuffle_epi32(vector, _MM_SHUFFLE(3, 2, 3, 2)));
				vector = _mm_max_epu8(vector, _mm_shuffle_epi32(vector, _MM_SHUFFLE(1, 1, 1, 1)));
				vector = _mm_max_epu8(vector, _mm_shufflelo_epi16(vector, _MM_SHUFFLE(1, 1, 1, 1)));
				vector = _mm_max_epu8(vector, _mm_srli_epi16(vector, 8));
				return u8(_mm_cvtsi128_si32(vector));
			};

			// 8-bit indices
			// Initialize the minima vector to all FFs (So 0xFF for each 8-bit lane)
			// And the maxima vector to all 0s (0 for each 8-bit lane)
			__m128i minima = _mm_set_epi32(0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu);
			__m128i maxima = _mm_set_epi32(0, 0, 0, 0);

			while (vertexCount >= vertsPerLoop) {
				const __m128i data = _mm_loadu_si128(reinterpret_cast<const __m128i*>(indexBuffer));
				minima = _mm_min_epu8(data, minima);
				maxima = _mm_max_epu8(data, maxima);

				indexBuffer += 16;
				vertexCount -= vertsPerLoop;
			}

			minimumIndex = u16(horizontalMin8(minima));
			maximumIndex = u16(horizontalMax8(maxima));
		}

		// If any indices could not be processed cause the buffer size
		// is not 16-byte aligned, process them the naive way
		// Calculate the minimum and maximum indices used in the index
		// buffer, so we'll only upload them
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

	// Analyzes a PICA index buffer to get the minimum and maximum indices in the
	// buffer, and returns them in a pair in the form [min, max]. Takes a template
	// parameter to decide whether the indices in the buffer are u8 or u16
	template <bool useShortIndices>
	std::pair<u16, u16> analyze(u8* indexBuffer, u32 vertexCount) {
#if defined(PICA_SIMD_ARM64)
		return analyzeNEON<useShortIndices>(indexBuffer, vertexCount);
#elif defined(PICA_SIMD_X64) && (defined(__SSE4_1__) || defined(__AVX__))
		// Annoyingly, MSVC refuses to define __SSE4_1__ even when we're building with AVX
		return analyzeSSE4_1<useShortIndices>(indexBuffer, vertexCount);
#else
		return analyzePortable<useShortIndices>(indexBuffer, vertexCount);
#endif
	}

	// In some really unfortunate scenarios (eg Android Studio emulator), we don't have access to glDrawRangeElementsBaseVertex
	// So we need to subtract the base vertex index from every index in the index buffer ourselves
	// This is not really common, so we do it without SIMD for the moment, just to be able to run on Android Studio
	template <bool useShortIndices>
	void subtractBaseIndex(u8* indexBuffer, u32 vertexCount, u16 baseIndex) {
		// Calculate the minimum and maximum indices used in the index buffer, so we'll only upload them
		if constexpr (useShortIndices) {
			u16* indexBuffer16 = reinterpret_cast<u16*>(indexBuffer);

			for (u32 i = 0; i < vertexCount; i++) {
				indexBuffer16[i] -= baseIndex;
			}
		} else {
			u8 baseIndex8 = u8(baseIndex);

			for (u32 i = 0; i < vertexCount; i++) {
				indexBuffer[i] -= baseIndex8;
			}
		}
	}
}  // namespace PICA::IndexBuffer
