#include "emulator.hpp"
#include "gl3w.h"

int main (int argc, char *argv[]) {
    Emulator emu;
    if (gl3wInit()) {
        Helpers::panic("Failed to initialize OpenGL");
    }

    auto elfPath = std::filesystem::current_path() / (argc > 1 ? argv[1] : "simple_tri.elf");
    if (!emu.loadELF(elfPath)) {
        Helpers::panic("Failed to load ELF file: %s", elfPath.c_str());
    }
    emu.run();
}