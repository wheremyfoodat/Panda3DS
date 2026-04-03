#pragma once

#ifdef IMGUI_FRONTEND

#include <SDL.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

#include "emulator.hpp"
#include "fsui/backend_sdl.hpp"
#include "fsui/fsui.hpp"
#include "panda_sdl/panda_fsui.hpp"

struct ImFont;

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
	void showPauseMenuFromController();

  private:
	bool useFullscreenUI();
	void drawDebugPanel();
	void drawClassicPausePanel();
	void drawClassicSettingsPanel();
	void drawSettingsGeneralSection(bool& reloadSettings);
	void drawSettingsWindowSection(bool& reloadSettings);
	void drawSettingsUISection(bool& reloadSettings);
	void drawSettingsGraphicsSection(bool& reloadSettings);
	void drawSettingsAudioSection(bool& reloadSettings);
	void drawSettingsBatterySection(bool& reloadSettings);
	void drawSettingsSDSection(bool& reloadSettings);

	SDL_Window* window = nullptr;
	SDL_GLContext glContext = nullptr;
	Emulator& emu;
	fsui::FontStack fontStack;
	fsui::SdlImGuiBackend imguiBackend;
	PandaFsuiAdapter fsuiAdapter;

	bool showDebug = true;
	bool showPauseMenu = false;
	bool showSettings = false;
	bool isPaused = false;
	bool captureKeyboard = false;
	bool captureMouse = false;
	bool fullscreenSelectorMode = false;

	std::function<void(bool)> onPauseChange;
	std::function<void(bool)> onVsyncChange;
	std::function<void()> onExitToSelector;
};

#endif
