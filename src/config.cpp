#include "config.hpp"

#include <fstream>
#include <string>

#include "log_helpers.hpp"
#include "toml.hpp"

// Largely based on https://github.com/nba-emu/NanoBoyAdvance/blob/master/src/platform/core/src/config.cpp
// We are legally allowed, as per the author's wish, to use the above code without any licensing restrictions
// However we still want to follow the license as closely as possible and offer the proper attributions.

EmulatorConfig::EmulatorConfig(const std::filesystem::path& path) { load(path); }

void EmulatorConfig::load(const std::filesystem::path& path) {
	// If the configuration file does not exist, create it and return
	std::error_code error;
	if (!std::filesystem::exists(path, error)) {
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

			// Get renderer
			auto rendererName = toml::find_or<std::string>(gpu, "Renderer", "OpenGL");
			auto configRendererType = Renderer::typeFromString(rendererName);

			if (configRendererType.has_value()) {
				rendererType = configRendererType.value();
			} else {
				Helpers::warn("Invalid renderer specified: %s\n", rendererName.c_str());
				rendererType = RendererType::OpenGL;
			}

			shaderJitEnabled = toml::find_or<toml::boolean>(gpu, "EnableShaderJIT", false);
		}
	}
}

void EmulatorConfig::save(const std::filesystem::path& path) {
	toml::basic_value<toml::preserve_comments> data;

	std::error_code error;
	if (std::filesystem::exists(path, error)) {
		try {
			data = toml::parse<toml::preserve_comments>(path);
		} catch (const std::exception& ex) {
			Helpers::warn("Exception trying to parse config file. Exception: %s\n", ex.what());
			return;
		}
	} else {
		if (error) {
			Helpers::warn("Filesystem error accessing %s (error: %s)\n", path.string().c_str(), error.message().c_str());
		}
		printf("Saving new configuration file %s\n", path.string().c_str());
	}

	data["GPU"]["EnableShaderJIT"] = shaderJitEnabled;
	data["GPU"]["Renderer"] = std::string(Renderer::typeToString(rendererType));

	std::ofstream file(path, std::ios::out);
	file << data;
	file.close();
}
