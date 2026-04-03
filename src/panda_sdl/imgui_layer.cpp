#ifdef IMGUI_FRONTEND

#include "panda_sdl/imgui_layer.hpp"

#include <glad/gl.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <vector>

#include "helpers.hpp"
#include "imgui.h"
#include "version.hpp"

namespace
{
	static const char* const s_fsui_theme_labels[] = {
		"Dark",
		"Light",
	};

	static const char* const s_fsui_theme_values[] = {
		"Dark",
		"Light",
	};

	struct InstalledGame
	{
		std::string title;
		std::string id;
		std::filesystem::path path;
	};

	std::vector<InstalledGame> scanGamesInDirectory(const std::filesystem::path& dir)
	{
		std::vector<InstalledGame> games;
		std::error_code ec;
		if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) {
			return games;
		}

		for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
			if (ec || !entry.is_regular_file()) {
				continue;
			}

			std::string ext = entry.path().extension().string();
			std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			if (ext == ".cci" || ext == ".3ds" || ext == ".cxi" || ext == ".app" || ext == ".ncch" || ext == ".elf" || ext == ".axf" ||
				ext == ".3dsx") {
				games.push_back({
					.title = entry.path().stem().string(),
					.id = entry.path().filename().string(),
					.path = entry.path(),
				});
			}
		}
		return games;
	}

	std::vector<InstalledGame> scanAllGames()
	{
		std::vector<InstalledGame> all_games;
		auto append_games = [&](const std::filesystem::path& path) {
			auto games = scanGamesInDirectory(path);
			all_games.insert(all_games.end(), games.begin(), games.end());
		};

		append_games("E:/");
		append_games(std::filesystem::path("E:/") / "PANDA3DS");

		std::filesystem::path root_path;
		#ifdef __WINRT__
		char* base_path = SDL_GetPrefPath(nullptr, nullptr);
		#else
		char* base_path = SDL_GetBasePath();
		#endif
		if (base_path) {
			root_path = std::filesystem::path(base_path);
			SDL_free(base_path);
		}

		if (!root_path.empty()) {
			append_games(root_path);
			append_games(root_path / "PANDA3DS");
		}

		std::sort(all_games.begin(), all_games.end(), [](const InstalledGame& lhs, const InstalledGame& rhs) {
			if (lhs.title != rhs.title) {
				return lhs.title < rhs.title;
			}
			return lhs.path < rhs.path;
		});

		all_games.erase(
			std::unique(all_games.begin(), all_games.end(), [](const InstalledGame& lhs, const InstalledGame& rhs) {
				return lhs.path == rhs.path;
			}),
			all_games.end()
		);
		return all_games;
	}

	std::string primaryScanRootLabel()
	{
		#ifdef __WINRT__
		char* base_path = SDL_GetPrefPath(nullptr, nullptr);
		#else
		char* base_path = SDL_GetBasePath();
		#endif
		if (!base_path) {
			return "[SDL Path]";
		}
		std::string out(base_path);
		SDL_free(base_path);
		return out;
	}

	struct VersionInfo
	{
		std::string app_version;
		std::string git_revision;
	};

	bool isHexRevision(const std::string& value)
	{
		if (value.size() != 7) {
			return false;
		}
		for (char c : value) {
			if (!std::isxdigit(static_cast<unsigned char>(c))) {
				return false;
			}
		}
		return true;
	}

	VersionInfo splitVersionString(const char* version_string)
	{
		VersionInfo info{version_string ? version_string : "", ""};
		if (info.app_version.empty()) {
			info.git_revision = "unknown";
			return info;
		}

		const auto dot = info.app_version.find_last_of('.');
		if (dot != std::string::npos) {
			const std::string maybe_rev = info.app_version.substr(dot + 1);
			if (isHexRevision(maybe_rev)) {
				info.git_revision = maybe_rev;
				info.app_version = info.app_version.substr(0, dot);
				return info;
			}
		}

		info.git_revision = "embedded";
		return info;
	}
} // namespace

ImGuiLayer::ImGuiLayer(SDL_Window* window, SDL_GLContext context, Emulator& emu)
	: window(window), glContext(context), emu(emu), fsuiAdapter(window, emu)
{}

bool ImGuiLayer::useFullscreenUI()
{
	return emu.getConfig().frontendSettings.enableFullscreenUI;
}

void ImGuiLayer::init()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.IniFilename = nullptr;
	showDebug = emu.getConfig().frontendSettings.showImGuiDebugPanel;

	fsui::SdlImGuiBackendConfig backend_config;
	backend_config.window = window;
	backend_config.gl_context = glContext;
	backend_config.renderer_backend = fsui::RendererBackend::OpenGL;
	backend_config.glsl_version = "#version 410 core";
	if (!imguiBackend.initialize(backend_config)) {
		Helpers::panic("Failed to initialize the shared SDL ImGui backend");
	}

	if (!fsui::BuildDefaultFontStack(fsui::DescribeSdl2FontBootstrap(window), fontStack)) {
		Helpers::panic("Failed to load bundled fullscreen UI fonts");
	}
	fsuiAdapter.setPauseCallback([this](bool paused) {
		isPaused = paused;
		showPauseMenu = paused;
		if (onPauseChange) {
			onPauseChange(paused);
		}
	});
	fsuiAdapter.setVsyncCallback([this](bool enabled) {
		if (onVsyncChange) {
			onVsyncChange(enabled);
		}
	});
	fsuiAdapter.setExitToSelectorCallback([this]() {
		showPauseMenu = false;
		showSettings = false;
		if (onExitToSelector) {
			onExitToSelector();
		}
	});
	fsuiAdapter.initialize(fontStack);
}

void ImGuiLayer::shutdown()
{
	fsuiAdapter.shutdown(true);
	imguiBackend.shutdown();
	ImGui::DestroyContext();
}

void ImGuiLayer::processEvent(const SDL_Event& event)
{
	imguiBackend.processEvent(event);
}

void ImGuiLayer::beginFrame()
{
	imguiBackend.newFrame();
	ImGui::NewFrame();

	ImGuiIO& io = ImGui::GetIO();
	captureKeyboard = io.WantCaptureKeyboard;
	captureMouse = io.WantCaptureMouse;
}

void ImGuiLayer::render()
{
	int drawable_w = 0;
	int drawable_h = 0;
	SDL_GL_GetDrawableSize(window, &drawable_w, &drawable_h);
	if (drawable_w > 0 && drawable_h > 0) {
		glViewport(0, 0, drawable_w, drawable_h);
	}

	const auto classic_request = fsuiAdapter.consumeClassicUiRequest();
	if (classic_request != PandaFsuiAdapter::ClassicUiRequest::None) {
		fullscreenSelectorMode = false;
		showPauseMenu = (emu.romType != ROMType::None);
		showSettings = (classic_request == PandaFsuiAdapter::ClassicUiRequest::OpenSettings);
	}

	drawDebugPanel();
	if (useFullscreenUI()) {
		fsuiAdapter.setSelectorMode(false);
		fsuiAdapter.render();
		fsuiAdapter.consumeCommands();
	} else {
		drawClassicPausePanel();
		drawClassicSettingsPanel();
	}

	ImGui::Render();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	imguiBackend.renderDrawData();
}

void ImGuiLayer::handleHotkey(const SDL_Event& event)
{
	if (event.type != SDL_KEYDOWN) {
		return;
	}

	if (!useFullscreenUI()) {
		switch (event.key.keysym.sym) {
			case SDLK_F1:
				showDebug = !showDebug;
				emu.getConfig().frontendSettings.showImGuiDebugPanel = showDebug;
				break;
			case SDLK_F2: showSettings = !showSettings; break;
			case SDLK_F3:
			case SDLK_ESCAPE:
				showPauseMenu = !showPauseMenu;
				if (onPauseChange) {
					isPaused = showPauseMenu;
					onPauseChange(isPaused);
				}
				break;
			default: break;
		}
		return;
	}

	if (event.key.keysym.sym == SDLK_F2 && !fullscreenSelectorMode) {
		if (!fsuiAdapter.hasActiveWindow()) {
			fsuiAdapter.openPauseMenu();
		}
		fsuiAdapter.switchToSettings();
		return;
	}

	if ((event.key.keysym.sym == SDLK_F3 || event.key.keysym.sym == SDLK_ESCAPE) && !fullscreenSelectorMode) {
		if (!fsuiAdapter.hasActiveWindow()) {
			fsuiAdapter.openPauseMenu();
		} else {
			fsuiAdapter.returnToPreviousWindow();
		}
	}
}

void ImGuiLayer::showPauseMenuFromController()
{
	if (useFullscreenUI()) {
		if (!fullscreenSelectorMode) {
			fsuiAdapter.openPauseMenu();
		}
		return;
	}

	if (!showPauseMenu) {
		showPauseMenu = true;
		if (onPauseChange) {
			isPaused = true;
			onPauseChange(true);
		}
	}
}

std::optional<std::filesystem::path> ImGuiLayer::runGameSelector()
{
	showPauseMenu = false;
	showSettings = false;
	fullscreenSelectorMode = useFullscreenUI();
	std::optional<std::filesystem::path> selected_path;

	if (fullscreenSelectorMode) {
		fsuiAdapter.setSelectorMode(true);
	} else {
		fsuiAdapter.setSelectorMode(false);
	}

	std::vector<InstalledGame> games = scanAllGames();
	int selected = 0;

	while (!selected_path.has_value()) {
		if (!fullscreenSelectorMode && !games.empty()) {
			selected = std::clamp(selected, 0, static_cast<int>(games.size()) - 1);
		}

		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			processEvent(event);
			if (event.type == SDL_QUIT) {
				fsuiAdapter.setSelectorMode(false);
				fullscreenSelectorMode = false;
				return std::nullopt;
			}

			if (fullscreenSelectorMode) {
				continue;
			}

			if (showSettings) {
				if (event.type == SDL_KEYDOWN && (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_F2)) {
					showSettings = false;
				}
				if (event.type == SDL_CONTROLLERBUTTONDOWN &&
					(event.cbutton.button == SDL_CONTROLLER_BUTTON_B || event.cbutton.button == SDL_CONTROLLER_BUTTON_BACK)) {
					showSettings = false;
				}
				continue;
			}

			if (event.type == SDL_CONTROLLERBUTTONDOWN) {
				if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP && selected > 0) {
					selected--;
				}
				if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN && selected < static_cast<int>(games.size()) - 1) {
					selected++;
				}
				if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A && !games.empty()) {
					selected_path = games[selected].path;
				}
				if (event.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
					showSettings = true;
				}
			}

			if (event.type == SDL_KEYDOWN) {
				if (event.key.keysym.sym == SDLK_UP && selected > 0) {
					selected--;
				}
				if (event.key.keysym.sym == SDLK_DOWN && selected < static_cast<int>(games.size()) - 1) {
					selected++;
				}
				if ((event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) && !games.empty()) {
					selected_path = games[selected].path;
				}
				if (event.key.keysym.sym == SDLK_F2) {
					showSettings = true;
				}
				if (event.key.keysym.sym == SDLK_ESCAPE) {
					return std::nullopt;
				}
				if (event.key.keysym.sym == SDLK_F5) {
					games = scanAllGames();
				}
			}
		}

		imguiBackend.newFrame();
		ImGui::NewFrame();

		int drawable_w = 0;
		int drawable_h = 0;
		SDL_GL_GetDrawableSize(window, &drawable_w, &drawable_h);
		if (drawable_w > 0 && drawable_h > 0) {
			glViewport(0, 0, drawable_w, drawable_h);
			glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);
		}

		const auto classic_request = fsuiAdapter.consumeClassicUiRequest();
		if (classic_request != PandaFsuiAdapter::ClassicUiRequest::None) {
			fullscreenSelectorMode = false;
			fsuiAdapter.setSelectorMode(false);
			showSettings = (classic_request == PandaFsuiAdapter::ClassicUiRequest::OpenSettings);
			games = scanAllGames();
		}

		drawDebugPanel();
		if (fullscreenSelectorMode) {
			fsuiAdapter.render();
			fsuiAdapter.consumeCommands();
			if (auto fsui_selection = fsuiAdapter.consumeLaunchPath(); fsui_selection.has_value()) {
				selected_path = fsui_selection;
			}
			if (fsuiAdapter.consumeCloseSelector() && !selected_path.has_value()) {
				fsuiAdapter.setSelectorMode(false);
				fullscreenSelectorMode = false;
				return std::nullopt;
			}
		} else if (showSettings) {
			drawClassicSettingsPanel();
		} else if (games.empty()) {
			std::string root_label = primaryScanRootLabel();
			ImGui::SetNextWindowPos(ImVec2(drawable_w * 0.5f, drawable_h * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
			ImGui::SetNextWindowSize(ImVec2(std::min(620.0f, drawable_w * 0.9f), std::min(260.0f, drawable_h * 0.9f)), ImGuiCond_Always);
			ImGui::Begin("No ROMs Found", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
			ImGui::TextWrapped(
				"No ROM inserted!\n\n"
				"Please add ROMs (supported formats: .cci, .3ds, .cxi, .app, .ncch, .elf, .axf, .3dsx) to:\n"
				"E:/\nE:/PANDA3DS\n%s\n%s/PANDA3DS",
				root_label.c_str(), root_label.c_str()
			);
			ImGui::Dummy(ImVec2(0.0f, 8.0f));
			if (ImGui::Button("Retry", ImVec2(120.0f, 0.0f))) {
				games = scanAllGames();
			}
			ImGui::SameLine();
			if (ImGui::Button("Settings", ImVec2(120.0f, 0.0f))) {
				showSettings = true;
			}
			ImGui::End();
		} else {
			ImGui::SetNextWindowPos(ImVec2(drawable_w * 0.5f, drawable_h * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
			ImGui::SetNextWindowSize(ImVec2(std::min(800.0f, drawable_w * 0.9f), std::min(600.0f, drawable_h * 0.9f)), ImGuiCond_Always);
			ImGui::Begin("Select Game", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
			for (int i = 0; i < static_cast<int>(games.size()); i++) {
				char buffer[512];
				std::snprintf(buffer, sizeof(buffer), "%d: %s (ID: %s)", i + 1, games[i].title.c_str(), games[i].id.c_str());
				if (ImGui::Selectable(buffer, selected == i)) {
					selected = i;
				}
			}
			ImGui::Dummy(ImVec2(0.0f, 8.0f));
			ImGui::Separator();
			ImGui::Dummy(ImVec2(0.0f, 8.0f));
			if (ImGui::Button("Launch", ImVec2(120.0f, 0.0f))) {
				selected_path = games[selected].path;
			}
			ImGui::SameLine();
			if (ImGui::Button("Refresh", ImVec2(120.0f, 0.0f))) {
				games = scanAllGames();
			}
			ImGui::SameLine();
			if (ImGui::Button("Settings", ImVec2(120.0f, 0.0f))) {
				showSettings = true;
			}
			ImGui::End();
		}

		ImGui::Render();
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		imguiBackend.renderDrawData();
		SDL_Delay(16);
	}

	fsuiAdapter.setSelectorMode(false);
	fullscreenSelectorMode = false;
	showPauseMenu = false;
	isPaused = false;
	return selected_path;
}

void ImGuiLayer::drawDebugPanel()
{
	if (!showDebug) {
		return;
	}

	int win_w = 0;
	int win_h = 0;
	SDL_GL_GetDrawableSize(window, &win_w, &win_h);
	const float max_w = std::max(220.0f, win_w * 0.45f);
	const float max_h = std::max(120.0f, win_h * 0.45f);
	ImGui::SetNextWindowSizeConstraints(ImVec2(200.0f, 0.0f), ImVec2(max_w, max_h));
	ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.35f);

	ImGui::Begin("##DebugOverlay", nullptr,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings);
	ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + max_w - 20.0f);
	int major = 0;
	int minor = 0;
	glGetIntegerv(GL_MAJOR_VERSION, &major);
	glGetIntegerv(GL_MINOR_VERSION, &minor);
	const VersionInfo version_info = splitVersionString(PANDA3DS_VERSION);
	ImGui::Text("Context : GL %d.%d", major, minor);
	ImGui::Text("Driver  : %s", glGetString(GL_RENDERER));
	ImGui::Text("Version : %s", version_info.app_version.c_str());
	ImGui::Text("Revision: %s", version_info.git_revision.c_str());
	ImGui::Text("FPS     : %.1f", ImGui::GetIO().Framerate);
	ImGui::Text("Paused  : %s", isPaused ? "Yes" : "No");
	ImGui::PopTextWrapPos();
	ImGui::End();
}

void ImGuiLayer::drawClassicPausePanel()
{
	if (!showPauseMenu) {
		return;
	}

	int win_w = 0;
	int win_h = 0;
	SDL_GL_GetDrawableSize(window, &win_w, &win_h);

	ImGui::SetNextWindowSize(ImVec2(260.0f, 160.0f), ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImVec2(win_w * 0.5f, win_h * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::Begin("##PauseMenu", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
	ImGui::TextUnformatted("Game Paused");
	ImGui::Dummy(ImVec2(0.0f, 8.0f));

	if (ImGui::Button("Resume", ImVec2(-1.0f, 0.0f))) {
		showPauseMenu = false;
		if (onPauseChange) {
			isPaused = false;
			onPauseChange(false);
		}
	}
	if (ImGui::Button("Settings", ImVec2(-1.0f, 0.0f))) {
		showSettings = true;
	}
	if (ImGui::Button("Quit", ImVec2(-1.0f, 0.0f))) {
		if (onExitToSelector) {
			onExitToSelector();
		} else {
			SDL_Event quit {};
			quit.type = SDL_QUIT;
			SDL_PushEvent(&quit);
		}
	}
	ImGui::End();
}

void ImGuiLayer::drawSettingsGeneralSection(bool& reloadSettings)
{
	EmulatorConfig& cfg = emu.getConfig();
	const char* current_lang = EmulatorConfig::languageCodeToString(cfg.systemLanguage);
	reloadSettings |= ImGui::Checkbox("Enable Discord RPC", &cfg.discordRpcEnabled);
	reloadSettings |= ImGui::Checkbox("Print App Version", &cfg.printAppVersion);
	reloadSettings |= ImGui::Checkbox("Enable Circle Pad Pro", &cfg.circlePadProEnabled);
	reloadSettings |= ImGui::Checkbox("Enable Fastmem", &cfg.fastmemEnabled);
	reloadSettings |= ImGui::Checkbox("Use Portable Build", &cfg.usePortableBuild);

	struct LanguageOption {
		const char* label;
		const char* code;
	};
	static const LanguageOption language_options[] = {
		{"English (en)", "en"}, {"Japanese (ja)", "ja"}, {"French (fr)", "fr"}, {"German (de)", "de"},
		{"Italian (it)", "it"}, {"Spanish (es)", "es"}, {"Chinese (zh)", "zh"}, {"Korean (ko)", "ko"},
		{"Dutch (nl)", "nl"}, {"Portuguese (pt)", "pt"}, {"Russian (ru)", "ru"}, {"Taiwanese (tw)", "tw"},
	};
	static const char* language_labels[] = {
		"English (en)", "Japanese (ja)", "French (fr)", "German (de)",
		"Italian (it)", "Spanish (es)", "Chinese (zh)", "Korean (ko)",
		"Dutch (nl)", "Portuguese (pt)", "Russian (ru)", "Taiwanese (tw)",
	};
	int lang_index = 0;
	for (int i = 0; i < static_cast<int>(std::size(language_options)); i++) {
		if (std::strcmp(language_options[i].code, current_lang) == 0) {
			lang_index = i;
			break;
		}
	}
	if (ImGui::Combo("System Language", &lang_index, language_labels, static_cast<int>(std::size(language_labels)))) {
		cfg.systemLanguage = EmulatorConfig::languageCodeFromString(language_options[lang_index].code);
		reloadSettings = true;
	}
}

void ImGuiLayer::drawSettingsWindowSection(bool& reloadSettings)
{
	EmulatorConfig& cfg = emu.getConfig();
	reloadSettings |= ImGui::Checkbox("Show App Version", &cfg.windowSettings.showAppVersion);
	reloadSettings |= ImGui::Checkbox("Remember Position", &cfg.windowSettings.rememberPosition);
	ImGui::InputInt("Pos X", &cfg.windowSettings.x);
	ImGui::InputInt("Pos Y", &cfg.windowSettings.y);
	ImGui::InputInt("Width", &cfg.windowSettings.width);
	ImGui::InputInt("Height", &cfg.windowSettings.height);
}

void ImGuiLayer::drawSettingsUISection(bool& reloadSettings)
{
	EmulatorConfig& cfg = emu.getConfig();
	static const char* theme_labels[] = {"System", "Light", "Dark", "Greetings Cat", "Cream", "OLED"};
	static const FrontendSettings::Theme theme_values[] = {
		FrontendSettings::Theme::System, FrontendSettings::Theme::Light, FrontendSettings::Theme::Dark,
		FrontendSettings::Theme::GreetingsCat, FrontendSettings::Theme::Cream, FrontendSettings::Theme::Oled,
	};
	int theme_index = 0;
	for (int i = 0; i < static_cast<int>(std::size(theme_values)); i++) {
		if (cfg.frontendSettings.theme == theme_values[i]) {
			theme_index = i;
			break;
		}
	}
	if (ImGui::Combo("Theme", &theme_index, theme_labels, static_cast<int>(std::size(theme_labels)))) {
		cfg.frontendSettings.theme = theme_values[theme_index];
		reloadSettings = true;
	}

	int fsui_theme_index = 0;
	for (int i = 0; i < static_cast<int>(std::size(s_fsui_theme_values)); i++) {
		if (cfg.fsuiTheme == s_fsui_theme_values[i]) {
			fsui_theme_index = i;
			break;
		}
	}
	if (ImGui::Combo("Fullscreen UI Theme", &fsui_theme_index, s_fsui_theme_labels, static_cast<int>(std::size(s_fsui_theme_labels)))) {
		cfg.fsuiTheme = s_fsui_theme_values[fsui_theme_index];
	}

	static const char* icon_labels[] = {"Rpog", "Rsyn", "Rnap", "Rcow", "SkyEmu", "Runpog"};
	static const FrontendSettings::WindowIcon icon_values[] = {
		FrontendSettings::WindowIcon::Rpog, FrontendSettings::WindowIcon::Rsyn, FrontendSettings::WindowIcon::Rnap,
		FrontendSettings::WindowIcon::Rcow, FrontendSettings::WindowIcon::SkyEmu, FrontendSettings::WindowIcon::Runpog,
	};
	int icon_index = 0;
	for (int i = 0; i < static_cast<int>(std::size(icon_values)); i++) {
		if (cfg.frontendSettings.icon == icon_values[i]) {
			icon_index = i;
			break;
		}
	}
	if (ImGui::Combo("Window Icon", &icon_index, icon_labels, static_cast<int>(std::size(icon_labels)))) {
		cfg.frontendSettings.icon = icon_values[icon_index];
		reloadSettings = true;
	}

	bool show_debug_panel = cfg.frontendSettings.showImGuiDebugPanel;
	if (ImGui::Checkbox("Show ImGui Debug Panel", &show_debug_panel)) {
		cfg.frontendSettings.showImGuiDebugPanel = show_debug_panel;
		showDebug = show_debug_panel;
	}
	ImGui::Checkbox("Stretch Output To Window", &cfg.frontendSettings.stretchImGuiOutputToWindow);
	bool enable_fullscreen_ui = cfg.frontendSettings.enableFullscreenUI;
	if (ImGui::Checkbox("Switch To Fullscreen UI", &enable_fullscreen_ui)) {
		cfg.frontendSettings.enableFullscreenUI = enable_fullscreen_ui;
		if (enable_fullscreen_ui) {
			showSettings = false;
			fullscreenSelectorMode = (emu.romType == ROMType::None);
			fsuiAdapter.setSelectorMode(fullscreenSelectorMode);
			if (!fullscreenSelectorMode && showPauseMenu) {
				fsuiAdapter.openPauseMenu();
			}
			fsuiAdapter.switchToSettings();
		} else {
			showPauseMenu = (emu.romType != ROMType::None);
			showSettings = true;
		}
	}
	ImGui::TextWrapped("When enabled, Alber uses the fullscreen interface instead of the classic centered ImGui menus.");
}

void ImGuiLayer::drawSettingsGraphicsSection(bool& reloadSettings)
{
	EmulatorConfig& cfg = emu.getConfig();
	ImGui::TextUnformatted("Renderer: OpenGL 4.1");
	if (ImGui::Checkbox("Enable VSync", &cfg.vsyncEnabled)) {
		if (onVsyncChange) {
			onVsyncChange(cfg.vsyncEnabled);
		}
	}
	reloadSettings |= ImGui::Checkbox("Enable Shader JIT", &cfg.shaderJitEnabled);
	reloadSettings |= ImGui::Checkbox("Use Ubershaders", &cfg.useUbershaders);
	reloadSettings |= ImGui::Checkbox("Accurate Shader Mul", &cfg.accurateShaderMul);
	reloadSettings |= ImGui::Checkbox("Accelerate Shaders", &cfg.accelerateShaders);
	reloadSettings |= ImGui::Checkbox("Force Shadergen for Lighting", &cfg.forceShadergenForLights);
	reloadSettings |= ImGui::InputInt("Shadergen Light Threshold", &cfg.lightShadergenThreshold);
	cfg.lightShadergenThreshold = std::clamp(cfg.lightShadergenThreshold, 0, 8);
	reloadSettings |= ImGui::Checkbox("Hash Textures", &cfg.hashTextures);
	reloadSettings |= ImGui::Checkbox("Enable Renderdoc", &cfg.enableRenderdoc);

	static const char* layout_labels[] = {"Default", "Default Flipped", "Side By Side", "Side By Side Flipped"};
	static const ScreenLayout::Layout layout_values[] = {
		ScreenLayout::Layout::Default, ScreenLayout::Layout::DefaultFlipped, ScreenLayout::Layout::SideBySide,
		ScreenLayout::Layout::SideBySideFlipped,
	};
	int layout_index = 0;
	for (int i = 0; i < static_cast<int>(std::size(layout_values)); i++) {
		if (cfg.screenLayout == layout_values[i]) {
			layout_index = i;
			break;
		}
	}
	if (ImGui::Combo("Screen Layout", &layout_index, layout_labels, static_cast<int>(std::size(layout_labels)))) {
		cfg.screenLayout = layout_values[layout_index];
		reloadSettings = true;
	}
	reloadSettings |= ImGui::SliderFloat("Top Screen Size", &cfg.topScreenSize, 0.0f, 1.0f);
}

void ImGuiLayer::drawSettingsAudioSection(bool& reloadSettings)
{
	EmulatorConfig& cfg = emu.getConfig();
	reloadSettings |= ImGui::Checkbox("Enable Audio", &cfg.audioEnabled);
	reloadSettings |= ImGui::Checkbox("Mute Audio", &cfg.audioDeviceConfig.muteAudio);
	reloadSettings |= ImGui::SliderFloat("Volume", &cfg.audioDeviceConfig.volumeRaw, 0.0f, 2.0f);
	reloadSettings |= ImGui::Checkbox("Enable AAC Audio", &cfg.aacEnabled);
	reloadSettings |= ImGui::Checkbox("Print DSP Firmware", &cfg.printDSPFirmware);

	static const char* dsp_labels[] = {"Null", "Teakra", "HLE"};
	static const Audio::DSPCore::Type dsp_values[] = {Audio::DSPCore::Type::Null, Audio::DSPCore::Type::Teakra, Audio::DSPCore::Type::HLE};
	int dsp_index = 0;
	for (int i = 0; i < static_cast<int>(std::size(dsp_values)); i++) {
		if (cfg.dspType == dsp_values[i]) {
			dsp_index = i;
			break;
		}
	}
	if (ImGui::Combo("DSP Emulation", &dsp_index, dsp_labels, static_cast<int>(std::size(dsp_labels)))) {
		cfg.dspType = dsp_values[dsp_index];
		reloadSettings = true;
	}

	static const char* curve_labels[] = {"Cubic", "Linear"};
	static const AudioDeviceConfig::VolumeCurve curve_values[] = {
		AudioDeviceConfig::VolumeCurve::Cubic, AudioDeviceConfig::VolumeCurve::Linear
	};
	int curve_index = 0;
	for (int i = 0; i < static_cast<int>(std::size(curve_values)); i++) {
		if (cfg.audioDeviceConfig.volumeCurve == curve_values[i]) {
			curve_index = i;
			break;
		}
	}
	if (ImGui::Combo("Volume Curve", &curve_index, curve_labels, static_cast<int>(std::size(curve_labels)))) {
		cfg.audioDeviceConfig.volumeCurve = curve_values[curve_index];
		reloadSettings = true;
	}
}

void ImGuiLayer::drawSettingsBatterySection(bool& reloadSettings)
{
	EmulatorConfig& cfg = emu.getConfig();
	reloadSettings |= ImGui::Checkbox("Charger Plugged", &cfg.chargerPlugged);
	reloadSettings |= ImGui::SliderInt("Battery %", &cfg.batteryPercentage, 0, 100);
}

void ImGuiLayer::drawSettingsSDSection(bool& reloadSettings)
{
	EmulatorConfig& cfg = emu.getConfig();
	reloadSettings |= ImGui::Checkbox("Use Virtual SD", &cfg.sdCardInserted);
	reloadSettings |= ImGui::Checkbox("Write Protect SD", &cfg.sdWriteProtected);
}

void ImGuiLayer::drawClassicSettingsPanel()
{
	if (!showSettings) {
		return;
	}

	int win_w = 0;
	int win_h = 0;
	SDL_GL_GetDrawableSize(window, &win_w, &win_h);
	const float max_w = std::max(240.0f, win_w * 0.9f);
	const float max_h = std::max(200.0f, win_h * 0.9f);
	const float width = std::min(520.0f, max_w);
	const float height = std::min(640.0f * 0.65f, max_h);
	ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImVec2(win_w * 0.5f, win_h * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	EmulatorConfig& cfg = emu.getConfig();
	bool reload_settings = false;
	bool save_config = false;

	ImGui::Begin("Settings", &showSettings, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
	if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen)) {
		drawSettingsGeneralSection(reload_settings);
	}
	if (ImGui::CollapsingHeader("Window")) {
		drawSettingsWindowSection(reload_settings);
	}
	if (ImGui::CollapsingHeader("UI")) {
		drawSettingsUISection(reload_settings);
	}
	if (ImGui::CollapsingHeader("Graphics")) {
		drawSettingsGraphicsSection(reload_settings);
	}
	if (ImGui::CollapsingHeader("Audio")) {
		drawSettingsAudioSection(reload_settings);
	}
	if (ImGui::CollapsingHeader("Battery")) {
		drawSettingsBatterySection(reload_settings);
	}
	if (ImGui::CollapsingHeader("SD")) {
		drawSettingsSDSection(reload_settings);
	}

	ImGui::Separator();
	if (ImGui::Button("Save")) {
		save_config = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Close")) {
		showSettings = false;
	}
	ImGui::End();

	if (reload_settings) {
		emu.reloadSettings();
	}
	if (save_config) {
		cfg.save();
	}
}

#endif
