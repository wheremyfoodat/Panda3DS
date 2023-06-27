#pragma once

#include <filesystem>
#include <fstream>
#include <SDL.h>
#include <glad/gl.h>

#include "cpu.hpp"
#include "io_file.hpp"
#include "memory.hpp"
#include "opengl.hpp"
#include "PICA/gpu.hpp"

enum class ROMType {
    None, ELF, NCSD
};

class Emulator {
    CPU cpu;
    GPU gpu;
    Memory memory;
    Kernel kernel;

    SDL_Window* window;
    SDL_GLContext glContext;
    SDL_GameController* gameController;
    int gameControllerID;

    static constexpr u32 width = 400;
    static constexpr u32 height = 240 * 2; // * 2 because 2 screens
    ROMType romType = ROMType::None;
    bool running = true;

    // Keep the handle for the ROM here to reload when necessary and to prevent deleting it
    // This is currently only used for ELFs, NCSDs use the IOFile API instead
    std::ifstream loadedELF;
    NCSD loadedNCSD;

public:
    Emulator() : kernel(cpu, memory, gpu), cpu(memory, kernel), gpu(memory), memory(cpu.getTicksRef()) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
            Helpers::panic("Failed to initialize SDL2");
        }

        // Make SDL use consistent positional button mapping
        SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");

        if (SDL_Init(SDL_INIT_GAMECONTROLLER) < 0) {
            Helpers::warn("Failed to initialize SDL2 GameController: %s", SDL_GetError());
        }

        // Request OpenGL 4.1 Core (Max available on MacOS)
        // MacOS gets mad if we don't explicitly demand a core profile
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        window = SDL_CreateWindow("Alber", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_OPENGL);

        if (window == nullptr) {
            Helpers::panic("Window creation failed: %s", SDL_GetError());
        }

        glContext = SDL_GL_CreateContext(window);

        if (glContext == nullptr) {
            Helpers::panic("OpenGL context creation failed: %s", SDL_GetError());
        }

        if(!gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress))) {
            Helpers::panic("OpenGL init failed: %s", SDL_GetError());
        }

        if (SDL_WasInit(SDL_INIT_GAMECONTROLLER)) {
            gameController = SDL_GameControllerOpen(0);

            if (gameController != nullptr) {
                SDL_Joystick* stick = SDL_GameControllerGetJoystick(gameController);
                gameControllerID = SDL_JoystickInstanceID(stick);
            }
        }

        reset();
    }

    void step();
    void render();
    void reset();
    void run();
    void runFrame();

    bool loadROM(const std::filesystem::path& path);
    bool loadNCSD(const std::filesystem::path& path);
    bool loadELF(const std::filesystem::path& path);
    bool loadELF(std::ifstream& file);
    void initGraphicsContext() { gpu.initGraphicsContext(); }
};
