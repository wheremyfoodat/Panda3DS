#pragma once

#include <filesystem>
#include <fstream>

#include "cpu.hpp"
#include "memory.hpp"
#include "opengl.hpp"
#include "PICA/gpu.hpp"
#include "SFML/Window.hpp"
#include "SFML/Graphics.hpp"

enum class ROMType {
    None, ELF, Cart
};

class Emulator {
    CPU cpu;
    GPU gpu;
    Memory memory;
    Kernel kernel;

    sf::RenderWindow window;
    static constexpr u32 width = 400;
    static constexpr u32 height = 240 * 2; // * 2 because 2 screens
    ROMType romType = ROMType::None;

    // Keep the handle for the ROM here to reload when necessary and to prevent deleting it
    std::ifstream loadedROM;

public:
    Emulator() : window(sf::VideoMode(width, height), "Alber", sf::Style::Default, sf::ContextSettings(0, 0, 0, 4, 3)),
                 kernel(cpu, memory, gpu), cpu(memory, kernel), gpu(memory), memory(cpu.getTicksRef()) {
        reset();
        window.setActive(true);
    }

    void step();
    void render();
    void reset();
    void run();
    void runFrame();

    bool loadELF(std::filesystem::path& path);
    bool loadELF(std::ifstream& file);
    void initGraphicsContext() { gpu.initGraphicsContext(); }
};