#pragma once
#include <atomic>
#include <string>
#include <vector>

#include "audio/audio_device.hpp"
#include "miniaudio.h"

class MiniAudioDevice : public AudioDeviceInterface {
	static constexpr ma_uint32 sampleRate = 32768;  // 3DS sample rate
	static constexpr ma_uint32 channelCount = 2;    // Audio output is stereo

	bool initialized = false;

	ma_device device;
	ma_context context;
	ma_device_config deviceConfig;

	// Store the last stereo sample we output. We play this when underruning to avoid pops.
	std::vector<std::string> audioDevices;

  public:
	MiniAudioDevice(const AudioDeviceConfig& audioSettings);

	// If safe is on, we create a null audio device
	void init(Samples& samples, bool safe = false) override;
	void close() override;

	void start() override;
	void stop() override;

	bool isInitialized() const { return initialized; }
};