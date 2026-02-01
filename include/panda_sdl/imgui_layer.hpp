#pragma once

#ifdef IMGUI_FRONTEND

#include <SDL.h>

#include <functional>
#include <optional>

#include "emulator.hpp"

class ImGuiLayer {
  public:
	ImGuiLayer(SDL_Window* window, SDL_GLContext context, Emulator& emu);
	void init();
	void shutdown();
	void processEvent(const SDL_Event& event);
	void beginFrame();
	void render();
	void handleHotkey(const SDL_Event& event);
	std::optional<std::filesystem::path> runGameSelector();

	bool wantsCaptureKeyboard() const { return captureKeyboard; }
	bool wantsCaptureMouse() const { return captureMouse; }

	void setPaused(bool paused) { isPaused = paused; }
	void setPauseCallback(std::function<void(bool)> callback) { onPauseChange = std::move(callback); }
	void setVsyncCallback(std::function<void(bool)> callback) { onVsyncChange = std::move(callback); }
	void setExitToSelectorCallback(std::function<void()> callback) { onExitToSelector = std::move(callback); }

  private:
	void drawDebugPanel();
	void drawPausePanel();
	void drawSettingsPanel();

	SDL_Window* window = nullptr;
	SDL_GLContext glContext = nullptr;
	Emulator& emu;

	bool showDebug = true;
	bool showPauseMenu = false;
	bool showSettings = false;
	bool isPaused = false;
	bool captureKeyboard = false;
	bool captureMouse = false;

	std::function<void(bool)> onPauseChange;
	std::function<void(bool)> onVsyncChange;
	std::function<void()> onExitToSelector;
};

#endif
