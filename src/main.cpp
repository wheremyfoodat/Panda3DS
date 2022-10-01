#include "emulator.hpp"
#include "gl3w.h"

int main (int argc, char *argv[]) {
    Emulator emu;
    if (gl3wInit()) {
        Helpers::panic("Failed to initialize OpenGL");
    }

    emu.initGraphicsContext();

    auto elfPath = std::filesystem::current_path() / (argc > 1 ? argv[1] : "Metroid2.3ds");
    if (!emu.loadROM(elfPath)) {
        // For some reason just .c_str() doesn't show the proper path
        Helpers::panic("Failed to load ROM file: %s", elfPath.string().c_str());
    }

    emu.run();
}