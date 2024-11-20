#pragma once

#include "audio/hle_mixer.hpp"
#include "compiler_builtins.hpp"
#include "helpers.hpp"

#if defined(_M_AMD64) || defined(__x86_64__)
#define DSP_SIMD_X64
#include <immintrin.h>
#elif defined(_M_ARM64) || defined(__aarch64__)
#define DSP_SIMD_ARM64
#include <arm_neon.h>
#endif

// Optimized SIMD functions for mixing the stereo output of a DSP voice into a quadraphonic intermediate mix
namespace DSP::MixIntoQuad {
	using IntermediateMix = Audio::DSPMixer::IntermediateMix;
	using StereoFrame16 = Audio::DSPMixer::StereoFrame<s16>;

	// Non-SIMD, portable algorithm
	ALWAYS_INLINE static void mixPortable(IntermediateMix& mix, StereoFrame16& frame, const float* gains) {
		for (usize sampleIndex = 0; sampleIndex < Audio::samplesInFrame; sampleIndex++) {
			// Mono samples are in the format: (l, r)
			// When converting to quad, gain0 and gain2 are applied to the left sample, gain1 and gain3 to the right one
			mix[sampleIndex][0] += s32(frame[sampleIndex][0] * gains[0]);
			mix[sampleIndex][1] += s32(frame[sampleIndex][1] * gains[1]);
			mix[sampleIndex][2] += s32(frame[sampleIndex][0] * gains[2]);
			mix[sampleIndex][3] += s32(frame[sampleIndex][1] * gains[3]);
		}
	}

#if defined(DSP_SIMD_X64) && (defined(__SSE4_1__) || defined(__AVX__))
	ALWAYS_INLINE static void mixSSE4_1(IntermediateMix& mix, StereoFrame16& frame, const float* gains) {
		__m128 gains_ = _mm_load_ps(gains);

		for (usize sampleIndex = 0; sampleIndex < Audio::samplesInFrame; sampleIndex++) {
			// The stereo samples, repeated every 4 bytes inside the vector register
			__m128i stereoSamples = _mm_castps_si128(_mm_load1_ps((float*)&frame[sampleIndex][0]));

			__m128 currentFrame = _mm_cvtepi32_ps(_mm_cvtepi16_epi32(stereoSamples));
			__m128i offset = _mm_cvttps_epi32(_mm_mul_ps(currentFrame, gains_));
			__m128i intermediateMixPrev = _mm_load_si128((__m128i*)&mix[sampleIndex][0]);
			__m128i result = _mm_add_epi32(intermediateMixPrev, offset);
			_mm_store_si128((__m128i*)&mix[sampleIndex][0], result);
		}
	}
#endif

#ifdef DSP_SIMD_ARM64
	ALWAYS_INLINE static void mixNEON(IntermediateMix& mix, StereoFrame16& frame, const float* gains) {
		float32x4_t gains_ = vld1q_f32(gains);

		for (usize sampleIndex = 0; sampleIndex < Audio::samplesInFrame; sampleIndex++) {
			// Load l and r samples and repeat them every 4 bytes
			int32x4_t stereoSamples = vld1q_dup_s32((s32*)&frame[sampleIndex][0]);
			// Expand the bottom 4 s16 samples into an int32x4 with sign extension, then convert them to float32x4
			float32x4_t currentFrame = vcvtq_f32_s32(vmovl_s16(vget_low_s16(vreinterpretq_s16_s32(stereoSamples))));

			// Multiply samples by their respective gains, truncate the result, and add it into the intermediate mix buffer
			int32x4_t offset = vcvtq_s32_f32(vmulq_f32(currentFrame, gains_));
			int32x4_t intermediateMixPrev = vld1q_s32((s32*)&mix[sampleIndex][0]);
			int32x4_t result = vaddq_f32(intermediateMixPrev, offset);
			vst1q_s32((s32*)&mix[sampleIndex][0], result);
		}
	}
#endif

	// Mixes the stereo output of a DSP voice into a quadraphonic intermediate mix
	static void mix(IntermediateMix& mix, StereoFrame16& frame, const float* gains) {
#if defined(DSP_SIMD_ARM64)
		return mixNEON(mix, frame, gains);
#elif defined(DSP_SIMD_X64) && (defined(__SSE4_1__) || defined(__AVX__))
		return mixSSE4_1(mix, frame, gains);
#else
		return mixPortable(mix, frame, gains);
#endif
	}
}  // namespace DSP::MixIntoQuad