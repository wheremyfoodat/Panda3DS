#pragma once

#include <SDL.h>
#include <glad/gl.h>

#include <filesystem>
#include <fstream>

#include "PICA/gpu.hpp"
#include "cpu.hpp"
#include "crypto/aes_engine.hpp"
#include "io_file.hpp"
#include "memory.hpp"
#include "opengl.hpp"

enum class ROMType { None, ELF, NCSD };

class Emulator {
	CPU cpu;
	GPU gpu;
	Memory memory;
	Kernel kernel;
	Crypto::AESEngine aesEngine;

	SDL_Window* window;
	SDL_GLContext glContext;
	SDL_GameController* gameController;
	int gameControllerID;

	// Variables to keep track of whether the user is controlling the 3DS analog stick with their keyboard
	// This is done so when a gamepad is connected, we won't automatically override the 3DS analog stick settings with the gamepad's state
	// And so the user can still use the keyboard to control the analog
	bool keyboardAnalogX = false;
	bool keyboardAnalogY = false;

	static constexpr u32 width = 400;
	static constexpr u32 height = 240 * 2;  // * 2 because 2 screens
	ROMType romType = ROMType::None;
	bool running = true;

	// Keep the handle for the ROM here to reload when necessary and to prevent deleting it
	// This is currently only used for ELFs, NCSDs use the IOFile API instead
	std::ifstream loadedELF;
	NCSD loadedNCSD;

  public:
	Emulator();

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
