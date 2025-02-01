#pragma once
#include <atomic>
#include <string>
#include <vector>

#include "config.hpp"
#include "helpers.hpp"
#include "miniaudio.h"
#include "ring_buffer.hpp"

class MiniAudioDevice {
	public:
	using Samples = Common::RingBuffer<ma_int16, 0x2000 * 2>;
	static constexpr ma_uint32 sampleRate = 32768;  // 3DS sample rate
	static constexpr ma_uint32 channelCount = 2;    // Audio output is stereo

	ma_device device;
	ma_context context;
	ma_device_config deviceConfig;
	Samples* samples = nullptr;

	const AudioDeviceConfig& audioSettings;

	bool initialized = false;
	bool running = false;

	// Store the last stereo sample we output. We play this when underruning to avoid pops.
	std::array<s16, 2> lastStereoSample;
	std::vector<std::string> audioDevices;

  public:
	MiniAudioDevice(const AudioDeviceConfig& audioSettings);

	// If safe is on, we create a null audio device
	void init(Samples& samples, bool safe = false);
	void close();

	void start();
	void stop();

	bool isInitialized() const { return initialized; }
};