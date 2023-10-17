#pragma once

#include <SDL.h>

#include "emulator.hpp"

struct FrontendSDL {
	FrontendSDL();
	bool loadROM(const std::filesystem::path& path);
	void run();

	Emulator emu;
	SDL_Window* window = nullptr;
#ifdef PANDA3DS_ENABLE_OPENGL
	SDL_GLContext glContext;
#endif
	SDL_GameController* gameController = nullptr;
	int gameControllerID;
};