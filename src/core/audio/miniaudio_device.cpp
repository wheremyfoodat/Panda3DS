#include "audio/miniaudio_device.hpp"

#include "helpers.hpp"

static constexpr uint channelCount = 2;

MiniAudioDevice::MiniAudioDevice() : initialized(false), running(false), samples(nullptr) {}

void MiniAudioDevice::init(Samples& samples, bool safe) {
	this->samples = &samples;

	// Probe for device and available backends and initialize audio
	ma_backend backends[ma_backend_null + 1];
	unsigned count = 0;

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
			printf("Found audio backend: %s\n", ma_get_backend_name(backend));

			// TODO: Make backend selectable here
			found = true;
			//count = 1;
			//backends[0] = backend;
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
	deviceConfig.sampleRate = 32768;
	//deviceConfig.periodSizeInFrames = 64;
	//deviceConfig.periods = 16;
	deviceConfig.pUserData = this;
	deviceConfig.aaudio.usage = ma_aaudio_usage_game;
	deviceConfig.wasapi.noAutoConvertSRC = true;

	deviceConfig.dataCallback = [](ma_device* device, void* out, const void* input, ma_uint32 frameCount) {
		auto self = reinterpret_cast<MiniAudioDevice*>(device->pUserData);
		s16* output = reinterpret_cast<ma_int16*>(out);

		while (self->samples->size() < frameCount * channelCount) {}
		self->samples->pop(output, frameCount * 2);
	};

	if (ma_device_init(&context, &deviceConfig, &device) != MA_SUCCESS) {
		Helpers::warn("Unable to initialize audio device");
		initialized = false;
		return;
	}

	initialized = true;
	start();
}

void MiniAudioDevice::start() {
	if (!initialized) {
		Helpers::warn("MiniAudio device not initialize, won't start");
		return;
	}

	if (ma_device_start(&device) == MA_SUCCESS) {
		running = true;
	} else {
		running = false;
		Helpers::warn("Failed to start audio device");
	}
}