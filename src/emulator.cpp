#include "emulator.hpp"

void Emulator::reset() {
    memory.reset();
    cpu.reset();
    kernel.reset();

    // Reloading r13 and r15 needs to happen after everything has been reset
    // Otherwise resetting the kernel or cpu might nuke them
    cpu.setReg(13, VirtualAddrs::StackTop); // Set initial SP
    if (romType == ROMType::ELF) { // Reload ELF if we're using one
        loadELF(loadedROM);
    }
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
    loadedROM.open(path, std::ios_base::binary); // Open ROM in binary mode
    romType = ROMType::ELF;

    return loadELF(loadedROM);
}

bool Emulator::loadELF(std::ifstream& file) {
    // Rewind ifstream
    loadedROM.clear();
    loadedROM.seekg(0);

    std::optional<u32> entrypoint = memory.loadELF(loadedROM);
    if (!entrypoint.has_value())
        return false;

    cpu.setReg(15, entrypoint.value()); // Set initial PC
    return true;
}