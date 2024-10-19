#pragma once
#include <filesystem>

#include "audio/dsp_core.hpp"
#include "renderer.hpp"

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
	
	bool shaderJitEnabled = shaderJitDefault;
	bool useUbershaders = ubershaderDefault;
	bool accelerateShaders = accelerateShadersDefault;
	bool accurateShaderMul = false;
	bool discordRpcEnabled = false;

	// Toggles whether to force shadergen when there's more than N lights active and we're using the ubershader, for better performance
	bool forceShadergenForLights = true;
	int lightShadergenThreshold = 1;

	RendererType rendererType = RendererType::OpenGL;
	Audio::DSPCore::Type dspType = Audio::DSPCore::Type::Null;

	bool sdCardInserted = true;
	bool sdWriteProtected = false;
	bool usePortableBuild = false;

	bool audioEnabled = false;
	bool vsyncEnabled = true;
	
	bool enableRenderdoc = false;
	bool printAppVersion = true;
	bool appVersionOnWindow = false;

	bool chargerPlugged = true;
	// Default to 3% battery to make users suffer
	int batteryPercentage = 3;

	// Default ROM path to open in Qt and misc frontends
	std::filesystem::path defaultRomPath = "";
	std::filesystem::path filePath;

	EmulatorConfig(const std::filesystem::path& path);
	void load();
	void save();
};
