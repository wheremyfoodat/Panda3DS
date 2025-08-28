#pragma once
#include <filesystem>
#include <string>

#include "audio/dsp_core.hpp"
#include "frontend_settings.hpp"
#include "renderer.hpp"
#include "screen_layout.hpp"
#include "services/region_codes.hpp"

struct AudioDeviceConfig {
	// Audio curve to use for volumes between 0-100
	enum class VolumeCurve : int {
		Cubic = 0,   // Samples are scaled by volume ^ 3
		Linear = 1,  // Samples are scaled by volume
	};

	float volumeRaw = 1.0f;
	VolumeCurve volumeCurve = VolumeCurve::Cubic;

	bool muteAudio = false;

	float getVolume() const {
		if (muteAudio) {
			return 0.0f;
		}

		return volumeRaw;
	}

	static VolumeCurve volumeCurveFromString(std::string inString);
	static const char* volumeCurveToString(VolumeCurve curve);
};

// Remember to initialize every field here to its default value otherwise bad things will happen
struct EmulatorConfig {
	// Only enable the shader JIT by default on platforms where it's completely tested
#if defined(PANDA3DS_X64_HOST) || defined(PANDA3DS_ARM64_HOST)
	static constexpr bool shaderJitDefault = true;
#else
	static constexpr bool shaderJitDefault = false;
#endif

	// For now, use specialized shaders by default on MacOS as M1 drivers are buggy when using the ubershader, and on Android since mobile GPUs are
	// horrible. On other platforms we default to ubershader + shadergen fallback for lights
#if defined(__ANDROID__) || defined(__APPLE__)
	static constexpr bool ubershaderDefault = false;
#else
	static constexpr bool ubershaderDefault = true;
#endif
	static constexpr bool accelerateShadersDefault = true;
	static constexpr bool audioEnabledDefault = true;

	static constexpr bool enableFastmemDefault = true;
	static constexpr bool hashTexturesDefault = false;

	// We default to OpenGL on all platforms other than iOS
#if defined(PANDA3DS_IOS)
	static constexpr RendererType rendererDefault = RendererType::Metal;
#else
	static constexpr RendererType rendererDefault = RendererType::OpenGL;
#endif

	bool shaderJitEnabled = shaderJitDefault;
	bool useUbershaders = ubershaderDefault;
	bool accelerateShaders = accelerateShadersDefault;
	bool fastmemEnabled = enableFastmemDefault;
	bool hashTextures = hashTexturesDefault;

	ScreenLayout::Layout screenLayout = ScreenLayout::Layout::Default;
	float topScreenSize = 0.5;

	bool accurateShaderMul = false;
	bool discordRpcEnabled = false;

	// Toggles whether to force shadergen when there's more than N lights active and we're using the ubershader, for better performance
	bool forceShadergenForLights = true;
	int lightShadergenThreshold = 1;

	RendererType rendererType = rendererDefault;
	Audio::DSPCore::Type dspType = Audio::DSPCore::Type::HLE;

	bool sdCardInserted = true;
	bool sdWriteProtected = false;
	bool circlePadProEnabled = true;
	bool usePortableBuild = false;

	bool audioEnabled = audioEnabledDefault;
	bool vsyncEnabled = true;
	bool aacEnabled = true;  // Enable AAC audio?

	bool enableRenderdoc = false;
	bool printAppVersion = true;
	bool printDSPFirmware = false;

	bool chargerPlugged = true;
	// Default to 3% battery to make users suffer
	int batteryPercentage = 3;

	LanguageCodes systemLanguage = LanguageCodes::English;

	// Default ROM path to open in Qt and misc frontends
	std::filesystem::path defaultRomPath = "";
	std::filesystem::path filePath;

	// Frontend window settings
	struct WindowSettings {
		static constexpr int defaultX = 200;
		static constexpr int defaultY = 200;
		static constexpr int defaultWidth = 800;
		static constexpr int defaultHeight = 240 * 2;

		bool rememberPosition = false;  // Remember window position & size
		bool showAppVersion = false;

		int x = defaultX;
		int y = defaultY;
		int width = defaultHeight;
		int height = defaultHeight;
	};

	WindowSettings windowSettings;
	AudioDeviceConfig audioDeviceConfig;
	FrontendSettings frontendSettings;

	EmulatorConfig(const std::filesystem::path& path);
	void load();
	void save();

	static LanguageCodes languageCodeFromString(std::string inString);
	static const char* languageCodeToString(LanguageCodes code);
};
