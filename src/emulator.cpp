#include "emulator.hpp"

void Emulator::reset() {
    cpu.reset();
}

void Emulator::step() {}

void Emulator::render() {

}

void Emulator::run() {
    while (window.isOpen()) {
        runFrame();
        OpenGL::setClearColor(1.0, 0.0, 0.0, 1.0);
        OpenGL::setViewport(400, 240 * 2);
        OpenGL::disableScissor();
        OpenGL::clearColor();

        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                Helpers::panic("Bye :(");
            }
        }

        window.display();
    }
}

void Emulator::runFrame() {
    constexpr u32 freq = 268 * 1024 * 1024;
    for (u32 i = 0; i < freq; i += 2) {
        step();
    }
}

bool Emulator::loadELF(std::filesystem::path& path) {
    std::optional<u32> entrypoint = memory.loadELF(path);
    if (!entrypoint.has_value())
        return false;

    Helpers::panic("Entrypoint: %08X\n", entrypoint.value());
}