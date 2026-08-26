#include "audio/oboe_audio_device.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include "helpers.hpp"

OboeAudioDevice::OboeAudioDevice(const AudioDeviceConfig& audioSettings) : AudioDeviceInterface(nullptr, audioSettings) {
	running = false;
}

OboeAudioDevice::~OboeAudioDevice() { close(); }

void OboeAudioDevice::init(Samples& samples, bool safe) {
	this->samples = &samples;
	running = false;
	lastStereoSample = {0, 0};

	if (safe) {
		initialized = true;
		return;
	}

	oboe::AudioStreamBuilder builder;
	builder.setDirection(oboe::Direction::Output)
		->setPerformanceMode(oboe::PerformanceMode::LowLatency)
		->setSharingMode(oboe::SharingMode::Shared) // use Exclusive ?
		->setFormat(oboe::AudioFormat::I16)
		->setFormatConversionAllowed(true)
		->setChannelCount(channelCount)
		->setSampleRate(sampleRate)
		->setSampleRateConversionQuality(oboe::SampleRateConversionQuality::Medium)
		->setUsage(oboe::Usage::Game)
		->setCallback(this);

	oboe::Result result = builder.openStream(stream);
	if (result != oboe::Result::OK) {
		Helpers::warn("Oboe: Failed to open audio stream (%s)\n", oboe::convertToText(result));
		initialized = false;
		return;
	}

	initialized = true;
}

void OboeAudioDevice::start() {
	if (!initialized) {
		Helpers::warn("Oboe audio device not initialized, won't start");
		return;
	}

	if (!running && stream != nullptr) {
		oboe::Result result = stream->requestStart();
		if (result == oboe::Result::OK) {
			running = true;
		} else {
			Helpers::warn("Oboe: Failed to start audio stream (%s)\n", oboe::convertToText(result));
		}
	} else if (!running) {
		running = true;
	}
}

void OboeAudioDevice::stop() {
	if (!initialized) {
		Helpers::warn("Oboe audio device not initialized, can't stop");
		return;
	}

	if (running) {
		running = false;

		if (stream != nullptr) {
			oboe::Result result = stream->requestStop();
			if (result != oboe::Result::OK) {
				Helpers::warn("Oboe: Failed to stop audio stream (%s)\n", oboe::convertToText(result));
			}
		}
	}
}

void OboeAudioDevice::close() {
	stop();

	if (stream != nullptr) {
		stream->close();
		stream.reset();
	}

	initialized = false;
}

oboe::DataCallbackResult OboeAudioDevice::onAudioReady(oboe::AudioStream*, void* audioData, int32_t frameCount) {
	if (!running) {
		std::memset(audioData, 0, usize(frameCount) * channelCount * sizeof(s16));
		return oboe::DataCallbackResult::Continue;
	}

	s16* output = reinterpret_cast<s16*>(audioData);
	usize samplesWritten = samples->pop(output, usize(frameCount) * channelCount);

	if (samplesWritten != 0) {
		std::memcpy(&lastStereoSample[0], &output[(samplesWritten - 1) * 2], sizeof(lastStereoSample));
	}

	float audioVolume = audioSettings.getVolume();

	if (audioVolume != 1.0f) {
		s16* sample = output;

		if (audioVolume > 1.0f) {
			audioVolume = 0.6f + 20.0f * std::log10(audioVolume);

			constexpr s32 min = s32(std::numeric_limits<s16>::min());
			constexpr s32 max = s32(std::numeric_limits<s16>::max());

			for (usize i = 0; i < samplesWritten; i += 2) {
				s16 l = s16(std::clamp<s32>(s32(float(sample[0]) * audioVolume), min, max));
				s16 r = s16(std::clamp<s32>(s32(float(sample[1]) * audioVolume), min, max));

				*sample++ = l;
				*sample++ = r;
			}
		} else {
			if (audioSettings.volumeCurve == AudioDeviceConfig::VolumeCurve::Cubic) {
				audioVolume = audioVolume * audioVolume * audioVolume;
			}

			for (usize i = 0; i < samplesWritten; i += 2) {
				s16 l = s16(float(sample[0]) * audioVolume);
				s16 r = s16(float(sample[1]) * audioVolume);

				*sample++ = l;
				*sample++ = r;
			}
		}
	}

	{
		s16* pointer = &output[samplesWritten * 2];
		s16 l = lastStereoSample[0];
		s16 r = lastStereoSample[1];

		for (usize i = samplesWritten; i < usize(frameCount) * channelCount; i += 2) {
			*pointer++ = l;
			*pointer++ = r;
		}
	}

	return oboe::DataCallbackResult::Continue;
}

void OboeAudioDevice::onErrorAfterClose(oboe::AudioStream*, oboe::Result error) {
	Helpers::warn("Oboe: Stream disconnected (%s), attempting to reopen\n", oboe::convertToText(error));

	if (samples != nullptr) {
		bool wasRunning = running;
		init(*samples, false);

		if (initialized && wasRunning) {
			start();
		}
	}
}