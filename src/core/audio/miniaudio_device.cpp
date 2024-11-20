#include "audio/miniaudio_device.hpp"

#include "helpers.hpp"

MiniAudioDevice::MiniAudioDevice() : initialized(false), running(false), samples(nullptr) {}

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

	deviceConfig.dataCallback = [](ma_device* device, void* out, const void* input, ma_uint32 frameCount) {
		auto self = reinterpret_cast<MiniAudioDevice*>(device->pUserData);
		s16* output = reinterpret_cast<ma_int16*>(out);
		const usize maxSamples = std::min(self->samples->Capacity(), usize(frameCount * channelCount));

		// Wait until there's enough samples to pop
		while (self->samples->size() < maxSamples) {
			// If audio output is disabled from the emulator thread, make sure that this callback will return and not hang
			if (!self->running) {
				return;
			}
		}

		self->samples->pop(output, maxSamples);
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
