#include "emulator.hpp"

int main(int argc, char *argv[]) {
	if (argc < 2) {
		Helpers::panic("No ROM provided. Usage: Alber.exe <ROM here>");
    }

    Emulator emu;
    emu.initGraphicsContext();

    auto romPath = std::filesystem::current_path() / argv[1];
    if (!emu.loadROM(romPath)) {
        // For some reason just .c_str() doesn't show the proper path
        Helpers::panic("Failed to load ROM file: %s", romPath.string().c_str());
    }

    emu.run();
}