#include "emulator.hpp"

int main(int argc, char *argv[]) {
	Emulator emu;

	emu.initGraphicsContext();

	if (argc > 1) {
		auto romPath = std::filesystem::current_path() / argv[1];
		if (!emu.loadROM(romPath)) {
			// For some reason just .c_str() doesn't show the proper path
			Helpers::panic("Failed to load ROM file: %s", romPath.string().c_str());
		}
	} else {
		printf("No ROM inserted! Load a ROM by dragging and dropping it into the emulator window!\n");
	}

	emu.run();
}