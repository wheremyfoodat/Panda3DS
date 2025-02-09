#pragma once
#include <cstring>

#include "audio/audio_device_interface.hpp"

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

	void renderBatch(RenderBatchCallback callback) override {
		if (running) {
			static constexpr usize frameCount = 774;
			static constexpr usize channelCount = 2;
			static s16 audioBuffer[frameCount * channelCount];

			usize samplesWritten = 0;
			samplesWritten += samples->pop(audioBuffer, frameCount * channelCount);

			// Get the last sample for underrun handling
			if (samplesWritten != 0) {
				std::memcpy(&lastStereoSample[0], &audioBuffer[(samplesWritten - 1) * 2], sizeof(lastStereoSample));
			}

			// If underruning, copy the last output sample
			{
				s16* pointer = &audioBuffer[samplesWritten * 2];
				s16 l = lastStereoSample[0];
				s16 r = lastStereoSample[1];

				for (usize i = samplesWritten; i < frameCount; i++) {
					*pointer++ = l;
					*pointer++ = r;
				}
			}

			callback(audioBuffer, sizeof(audioBuffer) / (channelCount * sizeof(s16)));
		}
	}

	bool isInitialized() const { return initialized; }
};