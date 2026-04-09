#include "config.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <map>
#include <string>
#include <unordered_map>

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

	printf("Loading existing configuration file %s\n", path.string().c_str());
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

			printAppVersion = toml::find_or<toml::boolean>(general, "PrintAppVersion", true);
			circlePadProEnabled = toml::find_or<toml::boolean>(general, "EnableCirclePadPro", true);
			fastmemEnabled = toml::find_or<toml::boolean>(general, "EnableFastmem", enableFastmemDefault);
			systemLanguage = languageCodeFromString(toml::find_or<std::string>(general, "SystemLanguage", "en"));

			// Load recent games list
			if (general.contains("RecentGames") && general.at("RecentGames").is_array()) {
				const auto& recentsArray = general.at("RecentGames").as_array();
				recentlyPlayed.clear();

				for (const auto& item : recentsArray) {
					if (item.is_string()) {
						std::filesystem::path gamePath = toml::get<std::string>(item);

						recentlyPlayed.push_back(gamePath);
						if (recentlyPlayed.size() >= maxRecentGames) {
							break;
						}
					}
				}
			}
		}
	}

	if (data.contains("Window")) {
		auto windowResult = toml::expect<toml::value>(data.at("Window"));
		if (windowResult.is_ok()) {
			auto window = windowResult.unwrap();

			windowSettings.showAppVersion = toml::find_or<toml::boolean>(window, "AppVersionOnWindow", false);
			windowSettings.rememberPosition = toml::find_or<toml::boolean>(window, "RememberWindowPosition", false);

			windowSettings.x = toml::find_or<toml::integer>(window, "WindowPosX", WindowSettings::defaultX);
			windowSettings.y = toml::find_or<toml::integer>(window, "WindowPosY", WindowSettings::defaultY);
			windowSettings.width = toml::find_or<toml::integer>(window, "WindowWidth", WindowSettings::defaultWidth);
			windowSettings.height = toml::find_or<toml::integer>(window, "WindowHeight", WindowSettings::defaultHeight);
		}
	}

	if (data.contains("GPU")) {
		auto gpuResult = toml::expect<toml::value>(data.at("GPU"));
		if (gpuResult.is_ok()) {
			auto gpu = gpuResult.unwrap();

			// Get renderer
			auto rendererName = toml::find_or<std::string>(gpu, "Renderer", Renderer::typeToString(rendererDefault));
			auto configRendererType = Renderer::typeFromString(rendererName);

			if (configRendererType.has_value()) {
				rendererType = configRendererType.value();
			} else {
				Helpers::warn("Invalid renderer specified: %s\n", rendererName.c_str());
				rendererType = rendererDefault;
			}

			shaderJitEnabled = toml::find_or<toml::boolean>(gpu, "EnableShaderJIT", shaderJitDefault);
			vsyncEnabled = toml::find_or<toml::boolean>(gpu, "EnableVSync", true);
			useUbershaders = toml::find_or<toml::boolean>(gpu, "UseUbershaders", ubershaderDefault);
			accurateShaderMul = toml::find_or<toml::boolean>(gpu, "AccurateShaderMultiplication", false);
			accelerateShaders = toml::find_or<toml::boolean>(gpu, "AccelerateShaders", accelerateShadersDefault);

			forceShadergenForLights = toml::find_or<toml::boolean>(gpu, "ForceShadergenForLighting", true);
			lightShadergenThreshold = toml::find_or<toml::integer>(gpu, "ShadergenLightThreshold", 1);
			hashTextures = toml::find_or<toml::boolean>(gpu, "HashTextures", hashTexturesDefault);
			enableRenderdoc = toml::find_or<toml::boolean>(gpu, "EnableRenderdoc", false);

			auto screenLayoutName = toml::find_or<std::string>(gpu, "ScreenLayout", "Default");
			screenLayout = ScreenLayout::layoutFromString(screenLayoutName);
			topScreenSize = float(std::clamp(toml::find_or<toml::floating>(gpu, "TopScreenSize", 0.5), 0.0, 1.0));
		}
	}

	if (data.contains("Audio")) {
		auto audioResult = toml::expect<toml::value>(data.at("Audio"));
		if (audioResult.is_ok()) {
			auto audio = audioResult.unwrap();

			auto dspCoreName = toml::find_or<std::string>(audio, "DSPEmulation", "HLE");
			dspType = Audio::DSPCore::typeFromString(dspCoreName);

			audioEnabled = toml::find_or<toml::boolean>(audio, "EnableAudio", audioEnabledDefault);
			aacEnabled = toml::find_or<toml::boolean>(audio, "EnableAACAudio", true);
			printDSPFirmware = toml::find_or<toml::boolean>(audio, "PrintDSPFirmware", false);

			audioDeviceConfig.muteAudio = toml::find_or<toml::boolean>(audio, "MuteAudio", false);
			// Our volume ranges from 0.0 (muted) to 2.0 (boosted, using a logarithmic scale). 1.0 is the "default" volume, ie we don't adjust the PCM
			// samples at all.
			audioDeviceConfig.volumeRaw = float(std::clamp(toml::find_or<toml::floating>(audio, "AudioVolume", 1.0), 0.0, 2.0));
			audioDeviceConfig.volumeCurve = AudioDeviceConfig::volumeCurveFromString(toml::find_or<std::string>(audio, "VolumeCurve", "cubic"));
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

	if (data.contains("UI")) {
		auto uiResult = toml::expect<toml::value>(data.at("UI"));
		if (uiResult.is_ok()) {
			auto ui = uiResult.unwrap();

			frontendSettings.theme = FrontendSettings::themeFromString(toml::find_or<std::string>(ui, "Theme", "dark"));
			frontendSettings.icon = FrontendSettings::iconFromString(toml::find_or<std::string>(ui, "WindowIcon", "rpog"));
			frontendSettings.language = toml::find_or<std::string>(ui, "Language", "en");
			frontendSettings.showImGuiDebugPanel = toml::find_or<toml::boolean>(ui, "ShowImGuiDebugPanel", true);
			#ifdef IMGUI_FRONTEND
			frontendSettings.stretchImGuiOutputToWindow = toml::find_or<toml::boolean>(ui, "StretchImGuiOutputToWindow", true);
			#else
			frontendSettings.stretchImGuiOutputToWindow = toml::find_or<toml::boolean>(ui, "StretchImGuiOutputToWindow", false);
			#endif
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
	data["General"]["PrintAppVersion"] = printAppVersion;
	data["General"]["SystemLanguage"] = languageCodeToString(systemLanguage);
	data["General"]["EnableCirclePadPro"] = circlePadProEnabled;
	data["General"]["EnableFastmem"] = fastmemEnabled;

	toml::array recentsArray;
	for (const auto& gamePath : recentlyPlayed) {
		recentsArray.push_back(gamePath.string());
	}
	data["General"]["RecentGames"] = recentsArray;

	data["Window"]["AppVersionOnWindow"] = windowSettings.showAppVersion;
	data["Window"]["RememberWindowPosition"] = windowSettings.rememberPosition;
	data["Window"]["WindowPosX"] = windowSettings.x;
	data["Window"]["WindowPosY"] = windowSettings.y;
	data["Window"]["WindowWidth"] = windowSettings.width;
	data["Window"]["WindowHeight"] = windowSettings.height;

	data["GPU"]["EnableShaderJIT"] = shaderJitEnabled;
	data["GPU"]["Renderer"] = std::string(Renderer::typeToString(rendererType));
	data["GPU"]["EnableVSync"] = vsyncEnabled;
	data["GPU"]["AccurateShaderMultiplication"] = accurateShaderMul;
	data["GPU"]["UseUbershaders"] = useUbershaders;
	data["GPU"]["ForceShadergenForLighting"] = forceShadergenForLights;
	data["GPU"]["ShadergenLightThreshold"] = lightShadergenThreshold;
	data["GPU"]["AccelerateShaders"] = accelerateShaders;
	data["GPU"]["EnableRenderdoc"] = enableRenderdoc;
	data["GPU"]["HashTextures"] = hashTextures;
	data["GPU"]["ScreenLayout"] = std::string(ScreenLayout::layoutToString(screenLayout));
	data["GPU"]["TopScreenSize"] = double(topScreenSize);

	data["Audio"]["DSPEmulation"] = std::string(Audio::DSPCore::typeToString(dspType));
	data["Audio"]["EnableAudio"] = audioEnabled;
	data["Audio"]["EnableAACAudio"] = aacEnabled;
	data["Audio"]["MuteAudio"] = audioDeviceConfig.muteAudio;
	data["Audio"]["AudioVolume"] = double(audioDeviceConfig.volumeRaw);
	data["Audio"]["VolumeCurve"] = std::string(AudioDeviceConfig::volumeCurveToString(audioDeviceConfig.volumeCurve));
	data["Audio"]["PrintDSPFirmware"] = printDSPFirmware;

	data["Battery"]["ChargerPlugged"] = chargerPlugged;
	data["Battery"]["BatteryPercentage"] = batteryPercentage;

	data["SD"]["UseVirtualSD"] = sdCardInserted;
	data["SD"]["WriteProtectVirtualSD"] = sdWriteProtected;

	data["UI"]["Theme"] = std::string(FrontendSettings::themeToString(frontendSettings.theme));
	data["UI"]["WindowIcon"] = std::string(FrontendSettings::iconToString(frontendSettings.icon));
	data["UI"]["Language"] = frontendSettings.language;
	data["UI"]["ShowImGuiDebugPanel"] = frontendSettings.showImGuiDebugPanel;
	data["UI"]["StretchImGuiOutputToWindow"] = frontendSettings.stretchImGuiOutputToWindow;

	std::ofstream file(path, std::ios::out);
	file << data;
	file.close();
}

AudioDeviceConfig::VolumeCurve AudioDeviceConfig::volumeCurveFromString(std::string inString) {
	// Transform to lower-case to make the setting case-insensitive
	std::transform(inString.begin(), inString.end(), inString.begin(), [](unsigned char c) { return std::tolower(c); });

	if (inString == "cubic") {
		return VolumeCurve::Cubic;
	} else if (inString == "linear") {
		return VolumeCurve::Linear;
	}

	// Default to cubic curve
	return VolumeCurve::Cubic;
}

const char* AudioDeviceConfig::volumeCurveToString(AudioDeviceConfig::VolumeCurve curve) {
	switch (curve) {
		case VolumeCurve::Linear: return "linear";

		case VolumeCurve::Cubic:
		default: return "cubic";
	}
}

LanguageCodes EmulatorConfig::languageCodeFromString(std::string inString) {  // Transform to lower-case to make the setting case-insensitive
	std::transform(inString.begin(), inString.end(), inString.begin(), [](unsigned char c) { return std::tolower(c); });

	static const std::unordered_map<std::string, LanguageCodes> map = {
		{"ja", LanguageCodes::Japanese}, {"en", LanguageCodes::English},    {"fr", LanguageCodes::French},  {"de", LanguageCodes::German},
		{"it", LanguageCodes::Italian},  {"es", LanguageCodes::Spanish},    {"zh", LanguageCodes::Chinese}, {"ko", LanguageCodes::Korean},
		{"nl", LanguageCodes::Dutch},    {"pt", LanguageCodes::Portuguese}, {"ru", LanguageCodes::Russian}, {"tw", LanguageCodes::Taiwanese},
	};

	if (auto search = map.find(inString); search != map.end()) {
		return search->second;
	}

	// Default to English if no language code in our map matches
	return LanguageCodes::English;
}

const char* EmulatorConfig::languageCodeToString(LanguageCodes code) {
	static constexpr std::array<const char*, 12> codes = {
		"ja", "en", "fr", "de", "it", "es", "zh", "ko", "nl", "pt", "ru", "tw",
	};

	// Invalid country code, return english
	if (static_cast<u32>(code) > static_cast<u32>(LanguageCodes::Taiwanese)) {
		return "en";
	} else {
		return codes[static_cast<u32>(code)];
	}
}
void EmulatorConfig::addToRecentGames(const std::filesystem::path& path) {
	// Remove path if it's already in the list
	auto it = std::find(recentlyPlayed.begin(), recentlyPlayed.end(), path);
	if (it != recentlyPlayed.end()) {
		recentlyPlayed.erase(it);
	}

	recentlyPlayed.insert(recentlyPlayed.begin(), path);

	// Limit how many games can be saved
	if (recentlyPlayed.size() > maxRecentGames) {
		recentlyPlayed.resize(maxRecentGames);
	}
}
