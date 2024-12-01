#include "frontend_settings.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_map>

// Frontend setting serialization/deserialization functions

FrontendSettings::Theme FrontendSettings::themeFromString(std::string inString) {
	// Transform to lower-case to make the setting case-insensitive
	std::transform(inString.begin(), inString.end(), inString.begin(), [](unsigned char c) { return std::tolower(c); });

	static const std::unordered_map<std::string, Theme> map = {
		{"system", Theme::System}, {"light", Theme::Light}, {"dark", Theme::Dark}, {"greetingscat", Theme::GreetingsCat}, {"cream", Theme::Cream},
	};

	if (auto search = map.find(inString); search != map.end()) {
		return search->second;
	}

	// Default to dark theme
	return Theme::Dark;
}

const char* FrontendSettings::themeToString(Theme theme) {
	switch (theme) {
		case Theme::System: return "system";
		case Theme::Light: return "light";
		case Theme::GreetingsCat: return "greetingscat";
		case Theme::Cream: return "cream";

		case Theme::Dark:
		default: return "dark";
	}
}

FrontendSettings::WindowIcon FrontendSettings::iconFromString(std::string inString) {  // Transform to lower-case to make the setting case-insensitive
	std::transform(inString.begin(), inString.end(), inString.begin(), [](unsigned char c) { return std::tolower(c); });

	static const std::unordered_map<std::string, WindowIcon> map = {
		{"rpog", WindowIcon::Rpog}, {"rsyn", WindowIcon::Rsyn},
	};

	if (auto search = map.find(inString); search != map.end()) {
		return search->second;
	}

	// Default to the icon rpog icon
	return WindowIcon::Rpog;
}

const char* FrontendSettings::iconToString(WindowIcon icon) {
	switch (icon) {
		case WindowIcon::Rsyn: return "rsyn";

		case WindowIcon::Rpog:
		default: return "rpog";
	}
}