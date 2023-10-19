#pragma once

#include <SDL.h>

#include "emulator.hpp"

class FrontendSDL {
	Emulator emu;
#ifdef PANDA3DS_ENABLE_OPENGL
	SDL_GLContext glContext;
#endif

  public:
	FrontendSDL();
	bool loadROM(const std::filesystem::path& path);
	void run();

	SDL_Window* window = nullptr;
	SDL_GameController* gameController = nullptr;
	int gameControllerID;
};