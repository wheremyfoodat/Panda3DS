#pragma once
#include <atomic>
#include <string>
#include <vector>

#include "audio/audio_device.hpp"

class LibretroAudioDevice : public AudioDeviceInterface {
	bool initialized = false;

  public:
	LibretroAudioDevice(const AudioDeviceConfig& audioSettings) : AudioDeviceInterface(nullptr, audioSettings), initialized(false) {
		running = false;
	}

	void init(Samples& samples, bool safe = false) override {
		this->samples = &samples;

		initialized = true;
		running = false;
	}

	void close() override {
		initialized = false;
		running = false;
	};

	void start() override { running = true; }
	void stop() override { running = false; };

	bool isInitialized() const { return initialized; }
	bool isRunning() const { return running; }
};