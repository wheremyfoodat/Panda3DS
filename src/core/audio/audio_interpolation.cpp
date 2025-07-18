// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "audio/audio_interpolation.hpp"

#include <algorithm>

#include "helpers.hpp"

namespace Audio::Interpolation {
	// Calculations are done in fixed point with 24 fractional bits.
	// (This is not verified. This was chosen for minimal error.)
	static constexpr u64 scaleFactor = 1 << 24;
	static constexpr u64 scaleMask = scaleFactor - 1;

	/// Here we step over the input in steps of rate, until we consume all of the input.
	/// Three adjacent samples are passed to fn each step.
	template <typename Function>
	static void stepOverSamples(State& state, StereoBuffer16& input, float rate, StereoFrame16& output, usize& outputi, Function fn) {
		if (input.empty()) {
			return;
		}

		input.insert(input.begin(), {state.xn2, state.xn1});

		const u64 step_size = static_cast<u64>(rate * scaleFactor);
		u64 fposition = state.fposition;
		usize inputi = 0;

		while (outputi < output.size()) {
			inputi = static_cast<usize>(fposition / scaleFactor);

			if (inputi + 2 >= input.size()) {
				inputi = input.size() - 2;
				break;
			}

			u64 fraction = fposition & scaleMask;
			output[outputi++] = fn(fraction, input[inputi], input[inputi + 1], input[inputi + 2]);

			fposition += step_size;
		}

		state.xn2 = input[inputi];
		state.xn1 = input[inputi + 1];
		state.fposition = fposition - inputi * scaleFactor;

		input.erase(input.begin(), std::next(input.begin(), inputi + 2));
	}

	void none(State& state, StereoBuffer16& input, float rate, StereoFrame16& output, usize& outputi) {
		stepOverSamples(state, input, rate, output, outputi, [](u64 fraction, const auto& x0, const auto& x1, const auto& x2) { return x0; });
	}

	void linear(State& state, StereoBuffer16& input, float rate, StereoFrame16& output, usize& outputi) {
		// Note on accuracy: Some values that this produces are +/- 1 from the actual firmware.
		stepOverSamples(state, input, rate, output, outputi, [](u64 fraction, const auto& x0, const auto& x1, const auto& x2) {
			// This is a saturated subtraction. (Verified by black-box fuzzing.)
			s64 delta0 = std::clamp<s64>(x1[0] - x0[0], -32768, 32767);
			s64 delta1 = std::clamp<s64>(x1[1] - x0[1], -32768, 32767);

			return std::array<s16, 2>{
				static_cast<s16>(x0[0] + fraction * delta0 / scaleFactor),
				static_cast<s16>(x0[1] + fraction * delta1 / scaleFactor),
			};
		});
	}

	void polyphase(State& state, StereoBuffer16& input, float rate, StereoFrame16& output, usize& outputi) {
		linear(state, input, rate, output, outputi);
	}
}  // namespace Audio::Interpolation