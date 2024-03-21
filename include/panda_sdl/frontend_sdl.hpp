#pragma once

#include <SDL.h>

#include <filesystem>

#include "emulator.hpp"
#include "input_mappings.hpp"

class FrontendSDL {
	Emulator emu;
#ifdef PANDA3DS_ENABLE_OPENGL
	SDL_GLContext glContext;
#endif

  public:
	FrontendSDL();
	bool loadROM(const std::filesystem::path& path);
	void run();
	u32 getMapping(InputMappings::Scancode scancode) { return keyboardMappings.getMapping(scancode); }

	SDL_Window* window = nullptr;
	SDL_GameController* gameController = nullptr;
	InputMappings keyboardMappings;

	int gameControllerID;
	bool programRunning = true;
	
	// For tracking whether to update gyroscope
	// We bind gyro to right click + mouse movement
	bool holdingRightClick = false;

	// Variables to keep track of whether the user is controlling the 3DS analog stick with their keyboard
	// This is done so when a gamepad is connected, we won't automatically override the 3DS analog stick settings with the gamepad's state
	// And so the user can still use the keyboard to control the analog
	bool keyboardAnalogX = false;
	bool keyboardAnalogY = false;
};