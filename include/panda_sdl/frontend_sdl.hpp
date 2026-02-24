#pragma once

#include <SDL.h>

#include <filesystem>
#include <memory>
#include <optional>

#include "emulator.hpp"
#include "input_mappings.hpp"
#include "screen_layout.hpp"

class FrontendSDL {
	Emulator emu;
#ifdef PANDA3DS_ENABLE_OPENGL
	SDL_GLContext glContext;
#endif
#ifdef IMGUI_FRONTEND
	std::unique_ptr<class ImGuiLayer> imgui;
#endif

  public:
	FrontendSDL();
	#ifdef IMGUI_FRONTEND
	FrontendSDL(SDL_Window* existingWindow, SDL_GLContext existingContext);
	#endif
	~FrontendSDL();
	#ifdef IMGUI_FRONTEND
	struct ImGuiWindowContext {
		SDL_Window* window = nullptr;
		SDL_GLContext context = nullptr;
	};
	static ImGuiWindowContext createImGuiWindowContext(const EmulatorConfig& bootConfig, const char* windowTitle);
	#endif
	bool loadROM(const std::filesystem::path& path);
	void run();
	std::optional<std::filesystem::path> selectGame();
	#ifdef IMGUI_FRONTEND
	bool consumeReturnToSelector();
	#endif
	u32 getMapping(InputMappings::Scancode scancode) { return keyboardMappings.getMapping(scancode); }

	SDL_Window* window = nullptr;
	SDL_GameController* gameController = nullptr;
	InputMappings keyboardMappings;

	u32 windowWidth = 400;
	u32 windowHeight = 480;
	int gameControllerID;
	bool programRunning = true;

	// Coordinates (x/y/width/height) for the two screens in window space, used for properly handling touchscreen regardless
	// of layout or resizing
	ScreenLayout::WindowCoordinates screenCoordinates;

	// For tracking whether to update gyroscope
	// We bind gyro to right click + mouse movement
	bool holdingRightClick = false;

	// Variables to keep track of whether the user is controlling the 3DS analog stick with their keyboard
	// This is done so when a gamepad is connected, we won't automatically override the 3DS analog stick settings with the gamepad's state
	// And so the user can still use the keyboard to control the analog
	bool keyboardAnalogX = false;
	bool keyboardAnalogY = false;
	bool emuPaused = false;
	bool returnToSelector = false;
	#ifdef IMGUI_FRONTEND
	bool controllerStartHeld = false;
	bool controllerSelectHeld = false;
	bool controllerPauseComboArmed = true;
	#endif

  private:
	void setupControllerSensors(SDL_GameController* controller);
	void handleLeftClick(int mouseX, int mouseY);
	void setPaused(bool paused);
	void togglePaused();
	void initialize(SDL_Window* existingWindow, SDL_GLContext existingContext, bool useExternalContext);
};