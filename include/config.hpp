#pragma once
#include <filesystem>

#include "renderer.hpp"

// Remember to initialize every field here to its default value otherwise bad things will happen
struct EmulatorConfig {
	bool shaderJitEnabled = false;
	RendererType rendererType = RendererType::OpenGL;

	EmulatorConfig(const std::filesystem::path& path);
	void load(const std::filesystem::path& path);
	void save(const std::filesystem::path& path);
};