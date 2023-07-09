#pragma once

#include <SDL.h>
#include <glad/gl.h>

#include <filesystem>
#include <fstream>
#include <optional>

#include "PICA/gpu.hpp"
#include "cpu.hpp"
#include "config.hpp"
#include "crypto/aes_engine.hpp"
#include "io_file.hpp"
#include "memory.hpp"
#include "gl_state.hpp"

enum class ROMType { None, ELF, NCSD, CXI };

enum class HttpAction { None, Screenshot, PressKey, ReleaseKey };

class Emulator {
	CPU cpu;
	GPU gpu;
	Memory memory;
	Kernel kernel;
	Crypto::AESEngine aesEngine;

	GLStateManager gl;
	EmulatorConfig config;
	SDL_Window* window;
	SDL_GLContext glContext;
	SDL_GameController* gameController = nullptr;
	int gameControllerID;

	// Variables to keep track of whether the user is controlling the 3DS analog stick with their keyboard
	// This is done so when a gamepad is connected, we won't automatically override the 3DS analog stick settings with the gamepad's state
	// And so the user can still use the keyboard to control the analog
	bool keyboardAnalogX = false;
	bool keyboardAnalogY = false;

	// For tracking whether to update gyroscope
	// We bind gyro to right click + mouse movement
	bool holdingRightClick = false;

	static constexpr u32 width = 400;
	static constexpr u32 height = 240 * 2;  // * 2 because 2 screens
	ROMType romType = ROMType::None;
	bool running = true;

#ifdef PANDA3DS_ENABLE_HTTP_SERVER
	std::atomic_bool pendingAction = false;
	HttpAction action = HttpAction::None;
	std::mutex actionMutex = {};
	u32 pendingKey = 0;
#endif

	// Keep the handle for the ROM here to reload when necessary and to prevent deleting it
	// This is currently only used for ELFs, NCSDs use the IOFile API instead
	std::ifstream loadedELF;
	NCSD loadedNCSD;

	std::optional<std::filesystem::path> romPath = std::nullopt;

  public:
	Emulator();
	~Emulator();

	void step();
	void render();
	void reset();
	void run();
	void runFrame();
	void screenshot(const std::string& name);

	bool loadROM(const std::filesystem::path& path);
	bool loadNCSD(const std::filesystem::path& path, ROMType type);
	bool loadELF(const std::filesystem::path& path);
	bool loadELF(std::ifstream& file);
	void initGraphicsContext();

#ifdef PANDA3DS_ENABLE_HTTP_SERVER
	void startHttpServer();
#endif
};
