#pragma once
#include <filesystem>

#include "audio/dsp_core.hpp"
#include "renderer.hpp"

// Remember to initialize every field here to its default value otherwise bad things will happen
struct EmulatorConfig {
	// Only enable the shader JIT by default on platforms where it's completely tested
#ifdef PANDA3DS_X64_HOST
	static constexpr bool shaderJitDefault = true;
#else
	static constexpr bool shaderJitDefault = false;
#endif

	bool shaderJitEnabled = shaderJitDefault;
	bool discordRpcEnabled = false;
	RendererType rendererType = RendererType::OpenGL;
	Audio::DSPCore::Type dspType = Audio::DSPCore::Type::Null;

	bool sdCardInserted = true;
	bool sdWriteProtected = false;
	bool usePortableBuild = false;

	bool audioEnabled = false;
	bool vsyncEnabled = true;

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