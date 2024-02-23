#pragma once
#include <atomic>
#include <string>
#include <vector>

#include "miniaudio.h"
#include "ring_buffer.hpp"

class MiniAudioDevice {
	using Samples = Common::RingBuffer<ma_int16, 1024>;
	static constexpr ma_uint32 sampleRate = 32768;  // 3DS sample rate
	static constexpr ma_uint32 channelCount = 2;    // Audio output is stereo

	ma_context context;
	ma_device_config deviceConfig;
	ma_device device;
	ma_resampler resampler;
	Samples* samples = nullptr;

	bool initialized = false;
	bool running = false;

	std::vector<std::string> audioDevices;
  public:
	MiniAudioDevice();
	// If safe is on, we create a null audio device
	void init(Samples& samples, bool safe = false);

	void start();
	void stop();
};