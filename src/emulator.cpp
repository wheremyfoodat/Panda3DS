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
        loadELF(loadedELF);
    }
}

void Emulator::step() {}

void Emulator::render() {

}

void Emulator::run() {
    while (running) {
        gpu.getGraphicsContext(); // Give the GPU a rendering context
        runFrame(); // Run 1 frame of instructions
        gpu.display(); // Display graphics

        // Send VBlank interrupts
        kernel.sendGPUInterrupt(GPUInterrupt::VBlank0);
        kernel.sendGPUInterrupt(GPUInterrupt::VBlank1);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                printf("Bye :(\n");
                running = false;
                return;
            }
        }

        SDL_GL_SwapWindow(window);
    }
}

void Emulator::runFrame() {
    cpu.runFrame();
}

bool Emulator::loadROM(const std::filesystem::path& path) {
    auto extension = path.extension();
    
    if (extension == ".elf" || extension == ".axf")
        return loadELF(path);
    else if (extension == ".3ds")
        return loadNCSD(path);
    else {
        printf("Unknown file type\n");
        return false;
    }
}

bool Emulator::loadNCSD(const std::filesystem::path& path) {
    romType = ROMType::NCSD;
    std::optional<NCSD> opt = memory.loadNCSD(path);

    if (!opt.has_value()) {
        return false;
    }

    loadedNCSD = opt.value();
    return true;
}

bool Emulator::loadELF(const std::filesystem::path& path) {
    loadedELF.open(path, std::ios_base::binary); // Open ROM in binary mode
    romType = ROMType::ELF;

    return loadELF(loadedELF);
}

bool Emulator::loadELF(std::ifstream& file) {
    // Rewind ifstream
    loadedELF.clear();
    loadedELF.seekg(0);

    std::optional<u32> entrypoint = memory.loadELF(loadedELF);
    if (!entrypoint.has_value())
        return false;

    cpu.setReg(15, entrypoint.value()); // Set initial PC
    if (entrypoint.value() & 1) {
        Helpers::panic("Misaligned ELF entrypoint. TODO: Check if ELFs can boot in thumb mode");
    }
    return true;
}