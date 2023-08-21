#pragma once
#include <filesystem>

#include "renderer.hpp"

// Remember to initialize every field here to its default value otherwise bad things will happen
struct EmulatorConfig {
	bool shaderJitEnabled = true;
	bool discordRpcEnabled = false;
	RendererType rendererType = RendererType::OpenGL;

	bool sdCardInserted = true;
	bool sdWriteProtected = false;

	bool chargerPlugged = true;
	// Default to 3% battery to make users suffer
	int batteryPercentage = 3;

	EmulatorConfig(const std::filesystem::path& path);
	void load(const std::filesystem::path& path);
	void save(const std::filesystem::path& path);
};