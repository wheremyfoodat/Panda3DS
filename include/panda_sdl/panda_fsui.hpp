#pragma once

#ifdef IMGUI_FRONTEND

#include <SDL.h>

#include <filesystem>
#include <functional>
#include <optional>
#include <vector>

#include "fsui/fsui.hpp"

class Emulator;

class PandaFsuiAdapter {
  public:
	enum class ClassicUiRequest {
		None,
		Return,
		OpenSettings,
	};

	PandaFsuiAdapter(SDL_Window* window, Emulator& emu);

	bool initialize(const fsui::FontStack& fonts);
	void shutdown(bool clear_state);
	void render();
	void consumeCommands();

	void setSelectorMode(bool selector_mode);
	bool isSelectorMode() const;
	bool hasActiveWindow() const;
	void openPauseMenu();
	void returnToPreviousWindow();
	void returnToMainWindow();
	void switchToSettings();

	std::optional<std::filesystem::path> consumeLaunchPath();
	bool consumeCloseSelector();
	ClassicUiRequest consumeClassicUiRequest();

	void setPauseCallback(std::function<void(bool)> callback);
	void setVsyncCallback(std::function<void(bool)> callback);
	void setExitToSelectorCallback(std::function<void()> callback);

  private:
	struct ParsedMetadata;
	struct LanguageOption;

	void syncUiStateFromConfig();
	void persistUiState(bool reload);
	void seedGameListPathsIfNeeded();
	std::vector<std::filesystem::path> currentScanRoots() const;
	bool hasSupportedExtension(const std::filesystem::path& path) const;
	std::vector<fsui::GameEntry> buildGameList();
	std::vector<fsui::SettingsPageDescriptor> buildSettingsPages(fsui::SettingsScope scope);
	fsui::CurrentGameInfo buildCurrentGameInfo();
	std::vector<fsui::MenuItemDescriptor> buildLandingItems();
	std::vector<fsui::MenuItemDescriptor> buildStartItems();
	std::vector<fsui::MenuItemDescriptor> buildExitItems();
	std::vector<fsui::MenuItemDescriptor> buildPauseItems();
	std::vector<fsui::MenuItemDescriptor> buildGameLaunchOptions(const fsui::GameEntry& entry);

	std::optional<ParsedMetadata> readMetadataForPath(const std::filesystem::path& path) const;
	fsui::GameEntry buildGameListEntry(const std::filesystem::directory_entry& entry) const;
	std::filesystem::path defaultCoverDirectory() const;
	std::filesystem::path findCoverPath(const std::filesystem::path& rom_path, const std::string& title_id) const;
	std::string currentGameTitle() const;
	std::string currentGameSubtitle() const;
	std::string formatPercent(float value) const;
	std::string formatInteger(int value) const;
	std::string formatTitleId(std::uint64_t program_id) const;
	void openFileAndLaunch();
	void requestLaunchPath(const std::filesystem::path& path);
	void requestClassicUi(bool open_settings);
	void openUnsupportedPrompt(std::string title, std::string message) const;

	SDL_Window* window = nullptr;
	Emulator& emu;
	fsui::UiContext fsuiContext;
	fsui::UiState uiState;
	std::vector<fsui::GameEntry> cachedGameList;
	std::optional<std::filesystem::path> pendingLaunchPath;
	bool closeSelectorRequested = false;
	ClassicUiRequest pendingClassicUiRequest = ClassicUiRequest::None;
	std::function<void(bool)> onPauseChange;
	std::function<void(bool)> onVsyncChange;
	std::function<void()> onExitToSelector;
};

#endif
