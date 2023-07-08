#include "config.hpp"

#include <fstream>

#include "helpers.hpp"
#include "toml.hpp"

// Largely based on https://github.com/nba-emu/NanoBoyAdvance/blob/master/src/platform/core/src/config.cpp
// We are legally allowed, as per the author's wish, to use the above code without any licensing restrictions
// However we still want to follow the license as closely as possible and offer the proper attributions.

void EmulatorConfig::load(const std::filesystem::path& path) {
	// If the configuration file does not exist, create it and return
	if (!std::filesystem::exists(path)) {
		save(path);
		return;
	}

	toml::value data;

	try {
		data = toml::parse(path);
	} catch (std::exception& ex) {
		Helpers::warn("Got exception trying to load config file. Exception: %s\n", ex.what());
		return;
	}

	if (data.contains("GPU")) {
		auto gpuResult = toml::expect<toml::value>(data.at("GPU"));
		if (gpuResult.is_ok()) {
			auto gpu = gpuResult.unwrap();

			shaderJitEnabled = toml::find_or<toml::boolean>(gpu, "EnableShaderJIT", false);
		}
	}
}

void EmulatorConfig::save(const std::filesystem::path& path) {
	toml::basic_value<toml::preserve_comments> data;

	if (std::filesystem::exists(path)) {
		try {
			data = toml::parse<toml::preserve_comments>(path);
		} catch (std::exception& ex) {
			Helpers::warn("Got exception trying to save config file. Exception: %s\n", ex.what());
			return;
		}
	}

	data["GPU"]["EnableShaderJIT"] = shaderJitEnabled;

	std::ofstream file(path, std::ios::out);
	file << data;
	file.close();
}