#pragma once
#include <array>

#include "config.hpp"
#include "helpers.hpp"
#include "ring_buffer.hpp"

class AudioDeviceInterface {
  protected:
	using Samples = Common::RingBuffer<s16, 0x2000 * 2>;
	Samples* samples = nullptr;

	const AudioDeviceConfig& audioSettings;

  public:
	AudioDeviceInterface(Samples* samples, const AudioDeviceConfig& audioSettings) : samples(samples), audioSettings(audioSettings) {}

	// Store the last stereo sample we output. We play this when underruning to avoid pops.
	// TODO: Make this protected again before merging!!!
	std::array<s16, 2> lastStereoSample{};

	bool running = false;
	Samples* getSamples() { return samples; }

	// If safe is on, we create a null audio device
	virtual void init(Samples& samples, bool safe = false) = 0;
	virtual void close() = 0;

	virtual void start() = 0;
	virtual void stop() = 0;
};