#include "config.hpp"

#include <cmath>
#include <fstream>
#include <map>
#include <string>

#include "helpers.hpp"
#include "toml.hpp"

// Largely based on https://github.com/nba-emu/NanoBoyAdvance/blob/master/src/platform/core/src/config.cpp
// We are legally allowed, as per the author's wish, to use the above code without any licensing restrictions
// However we still want to follow the license as closely as possible and offer the proper attributions.

EmulatorConfig::EmulatorConfig(const std::filesystem::path& path) : filePath(path) { load(); }

void EmulatorConfig::load() {
	const std::filesystem::path& path = filePath;

	// If the configuration file does not exist, create it and return
	std::error_code error;
	if (!std::filesystem::exists(path, error)) {
		save();
		return;
	}

	toml::value data;

	try {
		data = toml::parse(path);
	} catch (std::exception& ex) {
		Helpers::warn("Got exception trying to load config file. Exception: %s\n", ex.what());
		return;
	}

	if (data.contains("General")) {
		auto generalResult = toml::expect<toml::value>(data.at("General"));
		if (generalResult.is_ok()) {
			auto general = generalResult.unwrap();

			discordRpcEnabled = toml::find_or<toml::boolean>(general, "EnableDiscordRPC", false);
			usePortableBuild = toml::find_or<toml::boolean>(general, "UsePortableBuild", false);
			defaultRomPath = toml::find_or<std::string>(general, "DefaultRomPath", "");
		}
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

			shaderJitEnabled = toml::find_or<toml::boolean>(gpu, "EnableShaderJIT", shaderJitDefault);
			vsyncEnabled = toml::find_or<toml::boolean>(gpu, "EnableVSync", true);
		}
	}

	if (data.contains("Audio")) {
		auto audioResult = toml::expect<toml::value>(data.at("Audio"));
		if (audioResult.is_ok()) {
			auto audio = audioResult.unwrap();

			auto dspCoreName = toml::find_or<std::string>(audio, "DSPEmulation", "Null");
			dspType = Audio::DSPCore::typeFromString(dspCoreName);
			audioEnabled = toml::find_or<toml::boolean>(audio, "EnableAudio", false);
		}
	}

	if (data.contains("Battery")) {
		auto batteryResult = toml::expect<toml::value>(data.at("Battery"));
		if (batteryResult.is_ok()) {
			auto battery = batteryResult.unwrap();

			chargerPlugged = toml::find_or<toml::boolean>(battery, "ChargerPlugged", true);
			batteryPercentage = toml::find_or<toml::integer>(battery, "BatteryPercentage", 3);

			// Clamp battery % to [0, 100] to make sure it's a valid value
			batteryPercentage = std::clamp(batteryPercentage, 0, 100);
		}
	}

	if (data.contains("SD")) {
		auto sdResult = toml::expect<toml::value>(data.at("SD"));
		if (sdResult.is_ok()) {
			auto sd = sdResult.unwrap();

			sdCardInserted = toml::find_or<toml::boolean>(sd, "UseVirtualSD", true);
			sdWriteProtected = toml::find_or<toml::boolean>(sd, "WriteProtectVirtualSD", false);
		}
	}
}

void EmulatorConfig::save() {
	toml::basic_value<toml::preserve_comments, std::map> data;
	const std::filesystem::path& path = filePath;

	std::error_code error;
	if (std::filesystem::exists(path, error)) {
		try {
			data = toml::parse<toml::preserve_comments, std::map>(path);
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

	data["General"]["EnableDiscordRPC"] = discordRpcEnabled;
	data["General"]["UsePortableBuild"] = usePortableBuild;
	data["General"]["DefaultRomPath"] = defaultRomPath.string();
	data["GPU"]["EnableShaderJIT"] = shaderJitEnabled;
	data["GPU"]["Renderer"] = std::string(Renderer::typeToString(rendererType));
	data["GPU"]["EnableVSync"] = vsyncEnabled;
	data["Audio"]["DSPEmulation"] = std::string(Audio::DSPCore::typeToString(dspType));
	data["Audio"]["EnableAudio"] = audioEnabled;

	data["Battery"]["ChargerPlugged"] = chargerPlugged;
	data["Battery"]["BatteryPercentage"] = batteryPercentage;

	data["SD"]["UseVirtualSD"] = sdCardInserted;
	data["SD"]["WriteProtectVirtualSD"] = sdWriteProtected;

	std::ofstream file(path, std::ios::out);
	file << data;
	file.close();
}
