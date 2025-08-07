// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <deque>

#include "audio/hle_mixer.hpp"
#include "helpers.hpp"

namespace Audio::Interpolation {
	// A variable length buffer of signed PCM16 stereo samples.
	using StereoBuffer16 = std::deque<std::array<s16, 2>>;
	using StereoFrame16 = Audio::DSPMixer::StereoFrame<s16>;

	struct State {
		// Two history samples.
		std::array<s16, 2> xn1 = {};  // x[n-1]
		std::array<s16, 2> xn2 = {};  // x[n-2]
		// Current fractional position.
		u64 fposition = 0;
	};

	/**
	 * No interpolation. This is equivalent to a zero-order hold. There is a two-sample predelay.
	 * @param state Interpolation state.
	 * @param input Input buffer.
	 * @param rate Stretch factor. Must be a positive non-zero value.
	 *             rate > 1.0 performs decimation and rate < 1.0 performs upsampling.
	 * @param output The resampled audio buffer.
	 * @param outputi The index of output to start writing to.
	 */
	void none(State& state, StereoBuffer16& input, float rate, StereoFrame16& output, usize& outputi);

	/**
	 * Linear interpolation. This is equivalent to a first-order hold. There is a two-sample predelay.
	 * @param state Interpolation state.
	 * @param input Input buffer.
	 * @param rate Stretch factor. Must be a positive non-zero value.
	 *             rate > 1.0 performs decimation and rate < 1.0 performs upsampling.
	 * @param output The resampled audio buffer.
	 * @param outputi The index of output to start writing to.
	 */
	void linear(State& state, StereoBuffer16& input, float rate, StereoFrame16& output, usize& outputi);

	/**
	 * Polyphase interpolation. This is currently stubbed to just perform linear interpolation
	 * @param state Interpolation state.
	 * @param input Input buffer.
	 * @param rate Stretch factor. Must be a positive non-zero value.
	 *             rate > 1.0 performs decimation and rate < 1.0 performs upsampling.
	 * @param output The resampled audio buffer.
	 * @param outputi The index of output to start writing to.
	 */
	void polyphase(State& state, StereoBuffer16& input, float rate, StereoFrame16& output, usize& outputi);
}  // namespace Audio::Interpolation