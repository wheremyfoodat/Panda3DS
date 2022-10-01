#pragma once

#include <filesystem>
#include <fstream>
#include <SDL.h>

#include "cpu.hpp"
#include "memory.hpp"
#include "opengl.hpp"
#include "PICA/gpu.hpp"

enum class ROMType {
    None, ELF, Cart
};

class Emulator {
    CPU cpu;
    GPU gpu;
    Memory memory;
    Kernel kernel;

    SDL_Window* window;
    SDL_GLContext glContext;

    static constexpr u32 width = 400;
    static constexpr u32 height = 240 * 2; // * 2 because 2 screens
    ROMType romType = ROMType::None;
    bool running = true;

    // Keep the handle for the ROM here to reload when necessary and to prevent deleting it
    std::ifstream loadedROM;

public:
    Emulator() : kernel(cpu, memory, gpu), cpu(memory, kernel), gpu(memory), memory(cpu.getTicksRef()) {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            Helpers::panic("Failed to initialize SDL2");
        }

        // Request OpenGL 4.1 (Max available on MacOS)
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        window = SDL_CreateWindow("Alber", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_OPENGL);
        glContext = SDL_GL_CreateContext(window);
        reset();
    }

    void step();
    void render();
    void reset();
    void run();
    void runFrame();

    bool loadROM(const std::filesystem::path& path);
    bool loadELF(const std::filesystem::path& path);
    bool loadELF(std::ifstream& file);
    void initGraphicsContext() { gpu.initGraphicsContext(); }
};