#include "panda_sdl/frontend_sdl.hpp"

int main(int argc, char *argv[]) {
	FrontendSDL app;

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
}
