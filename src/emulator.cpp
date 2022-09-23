#include "emulator.hpp"

void Emulator::reset() {
    cpu.reset();
    gpu.reset();
    memory.reset();
    // Kernel must be reset last because it depends on CPU/Memory state
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
        // Clear window to black
        OpenGL::bindScreenFramebuffer();
        OpenGL::disableScissor();
        OpenGL::setClearColor(0.0, 0.0, 0.0, 1.0);
        OpenGL::clearColor();
        
        gpu.getGraphicsContext(); // Give the GPU a rendering context
        runFrame(); // Run 1 frame of instructions
        gpu.display();

        // Send VBlank interrupts
        kernel.sendGPUInterrupt(GPUInterrupt::VBlank0);
        kernel.sendGPUInterrupt(GPUInterrupt::VBlank1);
        window.display();

        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                printf("Bye :(\n");
                window.close();
                return;
            }
        }
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
    if (entrypoint.value() & 1) {
        Helpers::panic("Misaligned ELF entrypoint. TODO: Check if ELFs can boot in thumb mode");
    }
    return true;
}