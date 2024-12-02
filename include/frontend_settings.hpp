#pragma once
#include <string>

// Some UI settings that aren't fully frontend-dependent. Note: Not all frontends will support the same settings.
// Note: Any enums should ideally be ordered in the same order we want to show them in UI dropdown menus, so that we can cast indices to enums
// directly.
struct FrontendSettings {
	enum class Theme : int {
		System = 0,
		Light = 1,
		Dark = 2,
		GreetingsCat = 3,
		Cream = 4,
	};

	// Different panda-themed window icons
	enum class WindowIcon : int {
		Rpog = 0,
		Rsyn = 1,
		Rnap = 2,
		Rcow = 3,
		SkyEmu = 4,
	};

	Theme theme = Theme::Dark;
	WindowIcon icon = WindowIcon::Rpog;

	static Theme themeFromString(std::string inString);
	static const char* themeToString(Theme theme);

	static WindowIcon iconFromString(std::string inString);
	static const char* iconToString(WindowIcon icon);
};
