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

        // Update inputs in the HID module
        kernel.updateInputs();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                printf("Bye :(\n");
                running = false;
                return;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                case SDLK_z:        kernel.pressKey(1 << 0);        break;
                case SDLK_RIGHT:    kernel.pressKey(1 << 4);        break;
                case SDLK_LEFT:     kernel.pressKey(1 << 5);        break;
                case SDLK_UP:       kernel.pressKey(1 << 6);        break;
                case SDLK_DOWN:     kernel.pressKey(1 << 7);        break;
                case SDLK_w:        kernel.setCirclepadY(0x9C);     break;
                case SDLK_a:        kernel.setCirclepadX(-0x9C);    break;
                case SDLK_s:        kernel.setCirclepadY(-0x9C);    break;
                case SDLK_d:        kernel.setCirclepadX(0x9C);     break;
                case SDLK_j:        kernel.pressKey(1 << 1);        break;
                case SDLK_k:        kernel.pressKey(1 << 10);       break;
                case SDLK_l:        kernel.pressKey(1 << 11);       break;
                case SDLK_r:        kernel.pressKey(1 << 8);        break;
                }
                break;
            case SDL_KEYUP:
                switch (event.key.keysym.sym) {
                case SDLK_z:        kernel.releaseKey(1 << 0);  break;
                case SDLK_RIGHT:    kernel.releaseKey(1 << 4);  break;
                case SDLK_LEFT:     kernel.releaseKey(1 << 5);  break;
                case SDLK_UP:       kernel.releaseKey(1 << 6);  break;
                case SDLK_DOWN:     kernel.releaseKey(1 << 7);  break;
                case SDLK_w:        kernel.setCirclepadY(0);    break;
                case SDLK_a:        kernel.setCirclepadX(0);    break;
                case SDLK_s:        kernel.setCirclepadY(0);    break;
                case SDLK_d:        kernel.setCirclepadX(0);    break;
                case SDLK_j:        kernel.releaseKey(1 << 1);  break;
                case SDLK_k:        kernel.releaseKey(1 << 10); break;
                case SDLK_l:        kernel.releaseKey(1 << 11); break;
                case SDLK_r:        kernel.releaseKey(1 << 8);  break;
                }
                break;
            }
        }

        SDL_GL_SwapWindow(window);
    }
}

void Emulator::runFrame() {
    cpu.runFrame();
}

bool Emulator::loadROM(const std::filesystem::path& path) {
    // Get path for saving files (AppData on Windows, /home/user/.local/share/ApplcationName on Linux, etc)
    // Inside that path, we be use a game-specific folder as well. Eg if we were loading a ROM called PenguinDemo.3ds, the savedata would be in
    // %APPDATA%/Alber/PenguinDemo/SaveData on Windows, and so on. We do this because games save data in their own filesystem on the cart
    char* appData = SDL_GetPrefPath(nullptr, "Alber");
    const std::filesystem::path dataPath = std::filesystem::path(appData) / path.filename().stem();
    IOFile::setAppDataDir(dataPath);
    SDL_free(appData);

    kernel.initializeFS();
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
    cpu.setReg(15, loadedNCSD.entrypoint);

    if (loadedNCSD.entrypoint & 1) {
        Helpers::panic("Misaligned NCSD entrypoint; should this start the CPU in Thumb mode?");
    }

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