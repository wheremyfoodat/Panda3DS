#pragma once
#define NOMINMAX // Windows why
#include "cpu.hpp"
#include "helpers.hpp"
#include "opengl.hpp"
#include "SFML/Window.hpp"
#include "SFML/Graphics.hpp"

class Emulator {
    CPU cpu;
    sf::RenderWindow window;
    static constexpr u32 width = 400;
    static constexpr u32 height = 240 * 2; // * 2 because 2 screens

public:
    Emulator() : window(sf::VideoMode(width, height), "Alber", sf::Style::Default, sf::ContextSettings(0, 0, 0, 4, 3))  {
        reset();
        window.setActive(true);
    }

    void step();
    void render();
    void reset();
    void run();
    void runFrame();
};