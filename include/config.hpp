#pragma once
#include <filesystem>

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

	bool sdCardInserted = true;
	bool sdWriteProtected = false;
	bool usePortableBuild = false;

	bool chargerPlugged = true;
	// Default to 3% battery to make users suffer
	int batteryPercentage = 3;

	std::filesystem::path filePath;

	EmulatorConfig(const std::filesystem::path& path);
	void load();
	void save();
};