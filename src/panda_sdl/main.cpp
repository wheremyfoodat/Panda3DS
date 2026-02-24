#include "panda_sdl/frontend_sdl.hpp"

#ifdef IMGUI_FRONTEND
#include <SDL.h>
#include <glad/gl.h>

#include "config.hpp"
#include "version.hpp"
#endif

int emu_main(int argc, char *argv[]) {
	#ifdef IMGUI_FRONTEND
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
		Helpers::panic("Failed to initialize SDL2");
	}

	std::filesystem::path configPath = std::filesystem::current_path() / "config.toml";
	if (!std::filesystem::exists(configPath)) {
		#ifdef __WINRT__
		char* prefPath = SDL_GetPrefPath(nullptr, nullptr);
		#else
		char* prefPath = SDL_GetPrefPath(nullptr, "Alber");
		#endif
		if (prefPath) {
			configPath = std::filesystem::path(prefPath) / "config.toml";
			SDL_free(prefPath);
		}
	}

	EmulatorConfig bootConfig(configPath);
	const char* windowTitle = bootConfig.windowSettings.showAppVersion ? ("Alber v" PANDA3DS_VERSION) : "Alber";
	auto bootstrap = FrontendSDL::createImGuiWindowContext(bootConfig, windowTitle);
	FrontendSDL app(bootstrap.window, bootstrap.context);
	#else
	FrontendSDL app;
	#endif

	#ifdef IMGUI_FRONTEND
	std::optional<std::filesystem::path> forcedRom;
	if (argc > 1) {
		forcedRom = std::filesystem::current_path() / argv[1];
	}

	while (true) {
		std::optional<std::filesystem::path> selected = forcedRom;
		if (!selected.has_value()) {
			selected = app.selectGame();
			if (!selected.has_value()) {
				return 0;
			}
		}
		if (!app.loadROM(*selected)) {
			Helpers::panic("Failed to load ROM file: %s", selected->string().c_str());
		}
		forcedRom.reset();
		app.run();
		if (!app.consumeReturnToSelector()) {
			break;
		}
	}
	#else
	if (argc > 1) {
		auto romPath = std::filesystem::current_path() / argv[1];
		if (!app.loadROM(romPath)) {
			// For some reason just .c_str() doesn't show the proper path
			Helpers::panic("Failed to load ROM file: %s", romPath.string().c_str());
		}
	} else {
		printf("No ROM inserted! Load a ROM by dragging and dropping it into the emulator window!\n");
	}

	app.run();
	#endif
	return 0;
}

int main(int argc, char *argv[]) {
	return emu_main(argc, argv);
}

#ifdef __WINRT__
#include "SDL.h"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
	return SDL_WinRTRunApp(emu_main, nullptr);
}
#endif