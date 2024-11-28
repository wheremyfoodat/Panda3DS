#include "audio/miniaudio_device.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include "helpers.hpp"

MiniAudioDevice::MiniAudioDevice(AudioDeviceConfig& audioSettings)
	: initialized(false), running(false), samples(nullptr), audioSettings(audioSettings) {}

void MiniAudioDevice::init(Samples& samples, bool safe) {
	this->samples = &samples;
	running = false;

	// Probe for device and available backends and initialize audio
	ma_backend backends[ma_backend_null + 1];
	uint count = 0;

	if (safe) {
		backends[0] = ma_backend_null;
		count = 1;
	} else {
		bool found = false;
		for (uint i = 0; i <= ma_backend_null; i++) {
			ma_backend backend = ma_backend(i);
			if (!ma_is_backend_enabled(backend)) {
				continue;
			}

			backends[count++] = backend;

			// TODO: Make backend selectable here
			found = true;
			// count = 1;
			// backends[0] = backend;
		}

		if (!found) {
			initialized = false;

			Helpers::warn("No valid audio backend found\n");
			return;
		}
	}

	if (ma_context_init(backends, count, nullptr, &context) != MA_SUCCESS) {
		initialized = false;

		Helpers::warn("Unable to initialize audio context");
		return;
	}
	audioDevices.clear();

	struct UserContext {
		MiniAudioDevice* miniAudio;
		ma_device_config& config;
		bool found = false;
	};
	UserContext userContext = {.miniAudio = this, .config = deviceConfig};

	ma_context_enumerate_devices(
		&context,
		[](ma_context* pContext, ma_device_type deviceType, const ma_device_info* pInfo, void* pUserData) -> ma_bool32 {
			if (deviceType != ma_device_type_playback) {
				return true;
			}

			UserContext* userContext = reinterpret_cast<UserContext*>(pUserData);
			userContext->miniAudio->audioDevices.push_back(pInfo->name);

			// TODO: Check if this is the device we want here
			userContext->config.playback.pDeviceID = &pInfo->id;
			userContext->found = true;
			return true;
		},
		&userContext
	);

	if (!userContext.found) {
		Helpers::warn("MiniAudio: Device not found");
	}

	deviceConfig = ma_device_config_init(ma_device_type_playback);
	// The 3DS outputs s16 stereo audio @ 32768 Hz
	deviceConfig.playback.format = ma_format_s16;
	deviceConfig.playback.channels = channelCount;
	deviceConfig.sampleRate = sampleRate;
	// deviceConfig.periodSizeInFrames = 64;
	// deviceConfig.periods = 16;
	deviceConfig.pUserData = this;
	deviceConfig.aaudio.usage = ma_aaudio_usage_game;
	deviceConfig.wasapi.noAutoConvertSRC = true;

	lastStereoSample = {0, 0};

	deviceConfig.dataCallback = [](ma_device* device, void* out, const void* input, ma_uint32 frameCount) {
		auto self = reinterpret_cast<MiniAudioDevice*>(device->pUserData);
		if (!self->running) {
			return;
		}

		s16* output = reinterpret_cast<ma_int16*>(out);
		usize samplesWritten = 0;
		samplesWritten += self->samples->pop(output, frameCount * channelCount);

		// Get the last sample for underrun handling
		if (samplesWritten != 0) {
			std::memcpy(&self->lastStereoSample[0], &output[(samplesWritten - 1) * 2], sizeof(lastStereoSample));
		}

		// Adjust the volume of our samples based on the emulator's volume slider
		float audioVolume = self->audioSettings.getVolume();
		// If volume is 1.0 we don't need to do anything
		if (audioVolume != 1.0f) {
			s16* sample = output;

			// If our volume is > 1.0 then we boost samples using a logarithmic scale,
			// In this case we also have to clamp samples to make sure they don't wrap around
			if (audioVolume > 1.0f) {
				audioVolume = 0.6 + 20 * std::log10(audioVolume);

				constexpr s32 min = s32(std::numeric_limits<s16>::min());
				constexpr s32 max = s32(std::numeric_limits<s16>::max());

				for (usize i = 0; i < samplesWritten; i += 2) {
					s16 l = s16(std::clamp<s32>(s32(float(sample[0]) * audioVolume), min, max));
					s16 r = s16(std::clamp<s32>(s32(float(sample[1]) * audioVolume), min, max));

					*sample++ = l;
					*sample++ = r;
				}
			} else {
				// If our volume is in [0.0, 1.0) then just multiply by the volume. No need to clamp, since there is no danger of our samples wrapping
				// around due to overflow
				for (usize i = 0; i < samplesWritten; i += 2) {
					s16 l = s16(float(sample[0]) * audioVolume);
					s16 r = s16(float(sample[1]) * audioVolume);

					*sample++ = l;
					*sample++ = r;
				}
			}
		}

		// If underruning, copy the last output sample
		{
			s16* pointer = &output[samplesWritten * 2];
			s16 l = self->lastStereoSample[0];
			s16 r = self->lastStereoSample[1];

			for (usize i = samplesWritten; i < frameCount; i++) {
				*pointer++ = l;
				*pointer++ = r;
			}
		}
	};

	if (ma_device_init(&context, &deviceConfig, &device) != MA_SUCCESS) {
		Helpers::warn("Unable to initialize audio device");
		initialized = false;
		return;
	}

	initialized = true;
}

void MiniAudioDevice::start() {
	if (!initialized) {
		Helpers::warn("MiniAudio device not initialized, won't start");
		return;
	}

	// Ignore the call to start if the device is already running
	if (!running) {
		if (ma_device_start(&device) == MA_SUCCESS) {
			running = true;
		} else {
			Helpers::warn("Failed to start audio device");
		}
	}
}

void MiniAudioDevice::stop() {
	if (!initialized) {
		Helpers::warn("MiniAudio device not initialized, can't stop");
		return;
	}

	if (running) {
		running = false;

		if (ma_device_stop(&device) != MA_SUCCESS) {
			Helpers::warn("Failed to stop audio device");
		}
	}
}

void MiniAudioDevice::close() {
	stop();

	if (initialized) {
		initialized = false;

		ma_device_uninit(&device);
		ma_context_uninit(&context);
	}
}
