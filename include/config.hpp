#pragma once
#include <filesystem>

// Remember to initialize everything field here to its default value otherwise bad things will happen
struct EmulatorConfig {
	bool shaderJitEnabled = false;

	void load(const std::filesystem::path& path);
	void save(const std::filesystem::path& path);
};