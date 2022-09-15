#include "emulator.hpp"
#include "gl3w.h"

int main (int argc, char *argv[]) {
    Emulator emu;
    if (gl3wInit()) {
        Helpers::panic("Failed to initialize OpenGL");
    }

    emu.run();
}