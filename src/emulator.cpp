#include "emulator.hpp"

void Emulator::reset() {
    cpu.reset();
    kernel.reset();
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
    cpu.runFrame();
}

bool Emulator::loadELF(std::filesystem::path& path) {
    std::optional<u32> entrypoint = memory.loadELF(path);
    if (!entrypoint.has_value())
        return false;

    cpu.setReg(13, VirtualAddrs::StackTop); // Set initial SP
    cpu.setReg(15, entrypoint.value()); // Set initial PC
    return true;
}