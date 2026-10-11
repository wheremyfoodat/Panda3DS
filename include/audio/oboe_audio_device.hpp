#pragma once
#include <atomic>
#include <memory>

#include "audio/audio_device_interface.hpp"
#include <oboe/Oboe.h>


class OboeAudioDevice final : public AudioDeviceInterface, public oboe::AudioStreamCallback {
	static constexpr int sampleRate = 32768;  // 3DS sample rate
	static constexpr int channelCount = 2;    // Stereo

	bool initialized = false;
	std::shared_ptr<oboe::AudioStream> stream;

  public:
	OboeAudioDevice(const AudioDeviceConfig& audioSettings);
	~OboeAudioDevice() override;

	// If safe is on, we create a null audio device
	void init(Samples& samples, bool safe = false) override;
	void close() override;

	void start() override;
	void stop() override;

	bool isInitialized() const { return initialized; }

	oboe::DataCallbackResult onAudioReady(oboe::AudioStream* stream, void* audioData, int32_t frameCount) override;
	void onErrorAfterClose(oboe::AudioStream* stream, oboe::Result error) override;
};
