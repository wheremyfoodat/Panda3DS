#pragma once
#include <array>

#include "config.hpp"
#include "helpers.hpp"
#include "ring_buffer.hpp"

class AudioDeviceInterface {
  protected:
	static constexpr usize maxFrameCount = 0x2000;

	using Samples = Common::RingBuffer<s16, maxFrameCount * 2>;
	using RenderBatchCallback = usize (*)(const s16*, usize);

	Samples* samples = nullptr;

	const AudioDeviceConfig& audioSettings;
	// Store the last stereo sample we output. We play this when underruning to avoid pops.
	std::array<s16, 2> lastStereoSample{};

  public:
	AudioDeviceInterface(Samples* samples, const AudioDeviceConfig& audioSettings) : samples(samples), audioSettings(audioSettings) {}

	bool running = false;
	Samples* getSamples() { return samples; }

	// If safe is on, we create a null audio device
	virtual void init(Samples& samples, bool safe = false) = 0;
	virtual void close() = 0;

	virtual void start() = 0;
	virtual void stop() = 0;

	// Only used for audio devices that render multiple audio frames in one go, eg the libretro audio device.
	virtual void renderBatch(RenderBatchCallback callback) {}
};