#pragma once
#include <array>

#include "audio/dsp_shared_mem.hpp"
#include "helpers.hpp"

namespace Audio {
	using SampleFormat = HLE::SourceConfiguration::Configuration::Format;
	using SourceType = HLE::SourceConfiguration::Configuration::MonoOrStereo;

	class DSPMixer {
	  public:
		template <typename T, usize channelCount = 1>
		using Sample = std::array<T, channelCount>;

		template <typename T, usize channelCount>
		using Frame = std::array<Sample<T, channelCount>, 160>;

		template <typename T>
		using MonoFrame = Frame<T, 1>;

		template <typename T>
		using StereoFrame = Frame<T, 2>;

		template <typename T>
		using QuadFrame = Frame<T, 4>;

		// Internally the DSP uses four channels when mixing.
		// Neatly, QuadFrame<s32> means that every sample is a uint32x4 value, which is particularly nice for SIMD mixing
		using IntermediateMix = QuadFrame<s32>;

	  private:
		using ChannelFormat = HLE::DspConfiguration::OutputFormat;
		// The audio from each DSP voice is converted to quadraphonic and then fed into 3 intermediate mixing stages
		// Two of these intermediate mixers (second and third) are used for effects, including custom effects done on the CPU
		static constexpr usize mixerStageCount = 3;

	  public:
		ChannelFormat channelFormat = ChannelFormat::Stereo;
		std::array<float, mixerStageCount> volumes;
		std::array<bool, 2> enableAuxStages;

		void reset() {
			channelFormat = ChannelFormat::Stereo;

			volumes.fill(0.0);
			enableAuxStages.fill(false);
		}
	};
}  // namespace Audio