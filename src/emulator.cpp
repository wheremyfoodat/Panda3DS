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

        ServiceManager& srv = kernel.getServiceManager();

        // Send VBlank interrupts
        srv.sendGPUInterrupt(GPUInterrupt::VBlank0);
        srv.sendGPUInterrupt(GPUInterrupt::VBlank1);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            namespace Keys = HID::Keys;

            switch (event.type) {
            case SDL_QUIT:
                printf("Bye :(\n");
                running = false;
                return;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_l:     srv.pressKey(Keys::A);     break;
                    case SDLK_k:     srv.pressKey(Keys::B);     break;
                    case SDLK_o:     srv.pressKey(Keys::X);     break;
                    case SDLK_i:     srv.pressKey(Keys::Y);     break;

                    case SDLK_q:     srv.pressKey(Keys::L);     break;
                    case SDLK_p:     srv.pressKey(Keys::R);     break;

                    case SDLK_RIGHT: srv.pressKey(Keys::Right); break;
                    case SDLK_LEFT:  srv.pressKey(Keys::Left);  break;
                    case SDLK_UP:    srv.pressKey(Keys::Up);    break;
                    case SDLK_DOWN:  srv.pressKey(Keys::Down);  break;

                    case SDLK_w:     srv.setCirclepadY(0x9C);   break;
                    case SDLK_a:     srv.setCirclepadX(-0x9C);  break;
                    case SDLK_s:     srv.setCirclepadY(-0x9C);  break;
                    case SDLK_d:     srv.setCirclepadX(0x9C);   break;

                    case SDLK_RETURN: srv.pressKey(Keys::Start); break;
                    case SDLK_BACKSPACE: srv.pressKey(Keys::Select); break;
                }
                break;
            case SDL_KEYUP:
                switch (event.key.keysym.sym) {
                    case SDLK_l:     srv.releaseKey(Keys::A);     break;
                    case SDLK_k:     srv.releaseKey(Keys::B);     break;
                    case SDLK_o:     srv.releaseKey(Keys::X);     break;
                    case SDLK_i:     srv.releaseKey(Keys::Y);     break;

                    case SDLK_q:     srv.releaseKey(Keys::L);     break;
                    case SDLK_p:     srv.releaseKey(Keys::R);     break;

                    case SDLK_RIGHT: srv.releaseKey(Keys::Right); break;
                    case SDLK_LEFT:  srv.releaseKey(Keys::Left);  break;
                    case SDLK_UP:    srv.releaseKey(Keys::Up);    break;
                    case SDLK_DOWN:  srv.releaseKey(Keys::Down);  break;

                    // Err this is probably not ideal
                    case SDLK_w:     srv.setCirclepadY(0);  break;
                    case SDLK_a:     srv.setCirclepadX(0);  break;
                    case SDLK_s:     srv.setCirclepadY(0);  break;
                    case SDLK_d:     srv.setCirclepadX(0);  break;

                    case SDLK_RETURN: srv.releaseKey(Keys::Start); break;
                    case SDLK_BACKSPACE: srv.releaseKey(Keys::Select); break;
                }
                break;
            }
        }

        // Update inputs in the HID module
        srv.updateInputs();
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