#ifdef IMGUI_FRONTEND

#include "panda_sdl/imgui_layer.hpp"

#include <glad/gl.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iterator>

#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl.h"
#include "version.hpp"

namespace {
constexpr int kDebugPadding = 10;

struct InstalledGame {
	std::string title;
	std::string id;
	std::filesystem::path path;
};

std::vector<InstalledGame> scanGamesInDirectory(const std::filesystem::path& dir) {
	std::vector<InstalledGame> games;
	if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) return games;
	for (const auto& entry : std::filesystem::directory_iterator(dir)) {
		if (entry.is_regular_file()) {
			auto ext = entry.path().extension().string();
			if (ext == ".cci" || ext == ".3ds" || ext == ".cxi" || ext == ".app" || ext == ".ncch" || ext == ".elf" || ext == ".axf" ||
				ext == ".3dsx") {
				InstalledGame game;
				game.title = entry.path().stem().string();
				game.id = entry.path().filename().string();
				game.path = entry.path();
				games.push_back(game);
			}
		}
	}
	return games;
}

std::vector<InstalledGame> scanAllGames() {
	std::vector<InstalledGame> allGames;
	std::filesystem::path eRoot("E:/");
	{
		auto games = scanGamesInDirectory(eRoot);
		allGames.insert(allGames.end(), games.begin(), games.end());
	}
	{
		std::filesystem::path ePanda = eRoot / "PANDA3DS";
		auto games = scanGamesInDirectory(ePanda);
		allGames.insert(allGames.end(), games.begin(), games.end());
	}
	std::filesystem::path rootPath;
#ifdef __WINRT__
	{
		char* prefPath = SDL_GetPrefPath(nullptr, nullptr);
		if (prefPath) {
			rootPath = std::filesystem::path(prefPath);
			SDL_free(prefPath);
		}
	}
#else
	{
		char* basePath = SDL_GetBasePath();
		if (basePath) {
			rootPath = std::filesystem::path(basePath);
			SDL_free(basePath);
		}
	}
#endif
	if (!rootPath.empty()) {
		auto games = scanGamesInDirectory(rootPath);
		allGames.insert(allGames.end(), games.begin(), games.end());
		std::filesystem::path pandaPath = rootPath / "PANDA3DS";
		auto games2 = scanGamesInDirectory(pandaPath);
		allGames.insert(allGames.end(), games2.begin(), games2.end());
	}
	return allGames;
}

std::string primaryScanRootLabel() {
#ifdef __WINRT__
	char* prefPath = SDL_GetPrefPath(nullptr, nullptr);
	if (!prefPath) return "[SDL Pref Path]";
	std::string out(prefPath);
	SDL_free(prefPath);
	return out;
#else
	char* basePath = SDL_GetBasePath();
	if (!basePath) return "[SDL Base Path]";
	std::string out(basePath);
	SDL_free(basePath);
	return out;
#endif
}

struct VersionInfo {
	std::string appVersion;
	std::string gitRevision;
};

bool isHexRevision(const std::string& value) {
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

VersionInfo splitVersionString(const char* versionString) {
	VersionInfo info{versionString ? versionString : "", ""};
	if (info.appVersion.empty()) {
		info.gitRevision = "unknown";
		return info;
	}

	const auto dot = info.appVersion.find_last_of('.');
	if (dot != std::string::npos) {
		const std::string maybeRev = info.appVersion.substr(dot + 1);
		if (isHexRevision(maybeRev)) {
			info.gitRevision = maybeRev;
			info.appVersion = info.appVersion.substr(0, dot);
			return info;
		}
	}

	info.gitRevision = "embedded";
	return info;
}
}

ImGuiLayer::ImGuiLayer(SDL_Window* window, SDL_GLContext context, Emulator& emu) : window(window), glContext(context), emu(emu) {}

void ImGuiLayer::init() {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.IniFilename = nullptr;
	showDebug = emu.getConfig().frontendSettings.showImGuiDebugPanel;

	const bool sdlInitOk = ImGui_ImplSDL2_InitForOpenGL(window, glContext);
	const bool glInitOk = ImGui_ImplOpenGL3_Init("#version 410");
	#ifdef IMGUI_FRONTEND_DEBUG
	printf("[IMGUI] ImGui init: SDL2=%s OpenGL3=%s window=%p context=%p\n", sdlInitOk ? "ok" : "fail",
		glInitOk ? "ok" : "fail", window, glContext);
	if (!sdlInitOk || !glInitOk) {
		printf("[IMGUI] ImGui backend init failed\n");
	}
	#endif
}

void ImGuiLayer::shutdown() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
}

void ImGuiLayer::processEvent(const SDL_Event& event) {
	ImGui_ImplSDL2_ProcessEvent(&event);
}

void ImGuiLayer::beginFrame() {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame(window);
	ImGui::NewFrame();

	ImGuiIO& io = ImGui::GetIO();
	captureKeyboard = io.WantCaptureKeyboard;
	captureMouse = io.WantCaptureMouse;
}

void ImGuiLayer::render() {
	int drawableW = 0;
	int drawableH = 0;
	SDL_GL_GetDrawableSize(window, &drawableW, &drawableH);
	if (drawableW == 0 || drawableH == 0) {
		#ifdef IMGUI_FRONTEND_DEBUG
		static bool warnedOnce = false;
		if (!warnedOnce) {
			warnedOnce = true;
			printf("[IMGUI] render: drawable size is %dx%d\n", drawableW, drawableH);
		}
		#endif
	} else {
		glViewport(0, 0, drawableW, drawableH);
	}

	drawDebugPanel();
	drawPausePanel();
	drawSettingsPanel();

	ImGui::Render();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	#ifdef IMGUI_FRONTEND_DEBUG
	GLenum err = glGetError();
	if (err != GL_NO_ERROR) {
		printf("[IMGUI] render: glGetError=0x%X\n", err);
	}
	#endif
}

void ImGuiLayer::handleHotkey(const SDL_Event& event) {
	if (event.type != SDL_KEYDOWN) {
		return;
	}

	switch (event.key.keysym.sym) {
		case SDLK_F1:
			showDebug = !showDebug;
			emu.getConfig().frontendSettings.showImGuiDebugPanel = showDebug;
			break;
		case SDLK_F2:
			showSettings = !showSettings;
			break;
		case SDLK_F3:
		case SDLK_ESCAPE:
			showPauseMenu = !showPauseMenu;
			if (onPauseChange) {
				isPaused = showPauseMenu;
				onPauseChange(isPaused);
			}
			break;
		default:
			break;
	}
}

std::optional<std::filesystem::path> ImGuiLayer::runGameSelector() {
	std::vector<InstalledGame> games = scanAllGames();
	bool showNoRom = games.empty();
	bool inSettings = false;
	int selected = 0;
	bool selectionMade = false;
	std::optional<std::filesystem::path> selectedPath;

	while (!selectionMade) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			ImGui_ImplSDL2_ProcessEvent(&event);
			if (event.type == SDL_QUIT) {
				return std::nullopt;
			}
			if (!inSettings && event.type == SDL_CONTROLLERBUTTONDOWN) {
				if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP && selected > 0) selected--;
				if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN && selected < (int)games.size() - 1) selected++;
				if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A && !games.empty()) selectionMade = true;
			}
			if (!inSettings && event.type == SDL_KEYDOWN) {
				if (event.key.keysym.sym == SDLK_UP && selected > 0) selected--;
				if (event.key.keysym.sym == SDLK_DOWN && selected < (int)games.size() - 1) selected++;
				if (event.key.keysym.sym == SDLK_RETURN && !games.empty()) selectionMade = true;
			}
		}

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame(window);
		ImGui::NewFrame();

		int drawableW = 0;
		int drawableH = 0;
		SDL_GL_GetDrawableSize(window, &drawableW, &drawableH);
		#ifdef IMGUI_FRONTEND_DEBUG
		if (drawableW == 0 || drawableH == 0) {
			static bool warnedOnce = false;
			if (!warnedOnce) {
				warnedOnce = true;
				printf("[IMGUI] selector: drawable size is %dx%d\n", drawableW, drawableH);
			}
		}
		#endif

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;

		if (showNoRom) {
			std::string rootLabel = primaryScanRootLabel();
			ImGui::SetNextWindowPos(ImVec2(drawableW * 0.5f, drawableH * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
			ImGui::SetNextWindowSize(ImVec2(620, 260), ImGuiCond_Always);
			ImGui::Begin("No ROMs Found", nullptr, flags);
			ImGui::TextWrapped(
				"No ROM inserted!\n\n"
				"Please add ROMs (supported formats: .cci, .3ds, .cxi, .app, .ncch, .elf, .axf, .3dsx) to:\n"
				"E:/\nE:/PANDA3DS\n%s\n%s/PANDA3DS",
				rootLabel.c_str(), rootLabel.c_str()
			);
			ImGui::Dummy(ImVec2(0, 8));
			if (ImGui::Button("Retry", ImVec2(120, 0))) {
				games = scanAllGames();
				showNoRom = games.empty();
			}
			ImGui::SameLine();
			if (ImGui::Button("Settings", ImVec2(120, 0))) {
				inSettings = true;
				showSettings = true;
			}
			ImGui::End();
		} else if (!inSettings) {
			ImGui::SetNextWindowPos(ImVec2(drawableW * 0.5f, drawableH * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
			ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_Always);
			ImGui::Begin("Select Game", nullptr, flags);

			for (int i = 0; i < (int)games.size(); i++) {
				char buf[512];
				snprintf(buf, sizeof(buf), "%d: %s (ID: %s)", i + 1, games[i].title.c_str(), games[i].id.c_str());
				if (ImGui::Selectable(buf, selected == i)) selected = i;
			}
			ImGui::Dummy(ImVec2(0, 8));
			ImGui::Separator();
			ImGui::Dummy(ImVec2(0, 8));
			ImGui::SetCursorPosX((800 - 120) * 0.5f);
			if (ImGui::Button("Settings", ImVec2(120, 0))) {
				inSettings = true;
				showSettings = true;
			}
			ImGui::End();
		} else {
			drawSettingsPanel();
			if (!showSettings) {
				inSettings = false;
			}
		}

		if (selectionMade && !games.empty()) {
			selectedPath = games[selected].path;
		}

		ImGui::Render();
		SDL_GL_MakeCurrent(window, glContext);
		if (drawableW > 0 && drawableH > 0) {
			glViewport(0, 0, drawableW, drawableH);
			glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		#ifdef IMGUI_FRONTEND_DEBUG
		GLenum err = glGetError();
		if (err != GL_NO_ERROR) {
			printf("[IMGUI] selector: glGetError=0x%X\n", err);
		}
		#endif
		SDL_GL_SwapWindow(window);
		SDL_Delay(16);
	}

	return selectedPath;
}

void ImGuiLayer::drawDebugPanel() {
	if (!showDebug) {
		return;
	}

	ImGui::SetNextWindowPos(ImVec2(float(kDebugPadding), float(kDebugPadding)), ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.35f);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize |
							 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings;

	ImGui::Begin("##DebugOverlay", nullptr, flags);
	int major = 0;
	int minor = 0;
	glGetIntegerv(GL_MAJOR_VERSION, &major);
	glGetIntegerv(GL_MINOR_VERSION, &minor);
	const VersionInfo versionInfo = splitVersionString(PANDA3DS_VERSION);
	ImGui::Text("Context : GL %d.%d", major, minor);
	ImGui::Text("Driver  : %s", glGetString(GL_RENDERER));
	ImGui::Text("Version : %s", versionInfo.appVersion.c_str());
	ImGui::Text("Revision: %s", versionInfo.gitRevision.c_str());
	ImGui::Text("FPS     : %.1f", ImGui::GetIO().Framerate);
	ImGui::Text("Paused  : %s", isPaused ? "Yes" : "No");
	ImGui::End();
}

void ImGuiLayer::drawPausePanel() {
	if (!showPauseMenu) {
		return;
	}

	int winW = 0;
	int winH = 0;
	SDL_GL_GetDrawableSize(window, &winW, &winH);

	ImGui::SetNextWindowSize(ImVec2(260, 160), ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImVec2(winW * 0.5f, winH * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::Begin("##PauseMenu", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

	ImGui::TextUnformatted("Game Paused");
	ImGui::Dummy(ImVec2(0, 8));

	if (ImGui::Button("Resume", ImVec2(-1, 0))) {
		showPauseMenu = false;
		if (onPauseChange) {
			isPaused = false;
			onPauseChange(false);
		}
	}
	if (ImGui::Button("Settings", ImVec2(-1, 0))) {
		showSettings = true;
	}
	if (ImGui::Button("Quit", ImVec2(-1, 0))) {
		SDL_Event quit{};
		quit.type = SDL_QUIT;
		SDL_PushEvent(&quit);
	}

	ImGui::End();
}

void ImGuiLayer::drawSettingsPanel() {
	if (!showSettings) {
		return;
	}

	int winW = 0;
	int winH = 0;
	SDL_GL_GetDrawableSize(window, &winW, &winH);
	const float width = 520.0f;
	const float height = 640.0f * 0.65f;
	ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImVec2(winW * 0.5f, winH * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	EmulatorConfig& cfg = emu.getConfig();
	bool reloadSettings = false;
	bool saveConfig = false;
	const char* currentLang = EmulatorConfig::languageCodeToString(cfg.systemLanguage);

	ImGui::Begin("Settings", &showSettings, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

	if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen)) {
		reloadSettings |= ImGui::Checkbox("Enable Discord RPC", &cfg.discordRpcEnabled);
		reloadSettings |= ImGui::Checkbox("Print App Version", &cfg.printAppVersion);
		reloadSettings |= ImGui::Checkbox("Enable Circle Pad Pro", &cfg.circlePadProEnabled);
		reloadSettings |= ImGui::Checkbox("Enable Fastmem", &cfg.fastmemEnabled);
		reloadSettings |= ImGui::Checkbox("Use Portable Build", &cfg.usePortableBuild);

		struct LanguageOption {
			const char* label;
			const char* code;
		};
		static const LanguageOption languageOptions[] = {
			{"English (en)", "en"},    {"Japanese (ja)", "ja"},  {"French (fr)", "fr"},
			{"German (de)", "de"},     {"Italian (it)", "it"},   {"Spanish (es)", "es"},
			{"Chinese (zh)", "zh"},    {"Korean (ko)", "ko"},    {"Dutch (nl)", "nl"},
			{"Portuguese (pt)", "pt"}, {"Russian (ru)", "ru"},   {"Taiwanese (tw)", "tw"},
		};
		int langIndex = 0;
		for (int i = 0; i < (int)std::size(languageOptions); i++) {
			if (std::strcmp(languageOptions[i].code, currentLang) == 0) {
				langIndex = i;
				break;
			}
		}
		if (ImGui::Combo("System Language", &langIndex, [](void* data, int idx, const char** outText) {
			const auto* options = static_cast<const LanguageOption*>(data);
			*outText = options[idx].label;
			return true;
		}, (void*)languageOptions, (int)std::size(languageOptions))) {
			cfg.systemLanguage = EmulatorConfig::languageCodeFromString(languageOptions[langIndex].code);
			reloadSettings = true;
		}
	}

	if (ImGui::CollapsingHeader("Window")) {
		reloadSettings |= ImGui::Checkbox("Show App Version", &cfg.windowSettings.showAppVersion);
		reloadSettings |= ImGui::Checkbox("Remember Position", &cfg.windowSettings.rememberPosition);
		ImGui::InputInt("Pos X", &cfg.windowSettings.x);
		ImGui::InputInt("Pos Y", &cfg.windowSettings.y);
		ImGui::InputInt("Width", &cfg.windowSettings.width);
		ImGui::InputInt("Height", &cfg.windowSettings.height);
	}

	if (ImGui::CollapsingHeader("UI")) {
		static const char* themeLabels[] = {"System", "Light", "Dark", "Greetings Cat", "Cream", "OLED"};
		static const FrontendSettings::Theme themeValues[] = {
			FrontendSettings::Theme::System, FrontendSettings::Theme::Light, FrontendSettings::Theme::Dark,
			FrontendSettings::Theme::GreetingsCat, FrontendSettings::Theme::Cream, FrontendSettings::Theme::Oled,
		};
		int themeIndex = 0;
		for (int i = 0; i < (int)std::size(themeValues); i++) {
			if (cfg.frontendSettings.theme == themeValues[i]) {
				themeIndex = i;
				break;
			}
		}
		if (ImGui::Combo("Theme", &themeIndex, themeLabels, (int)std::size(themeLabels))) {
			cfg.frontendSettings.theme = themeValues[themeIndex];
			reloadSettings = true;
		}

		static const char* iconLabels[] = {"Rpog", "Rsyn", "Rnap", "Rcow", "SkyEmu", "Runpog"};
		static const FrontendSettings::WindowIcon iconValues[] = {
			FrontendSettings::WindowIcon::Rpog, FrontendSettings::WindowIcon::Rsyn,  FrontendSettings::WindowIcon::Rnap,
			FrontendSettings::WindowIcon::Rcow, FrontendSettings::WindowIcon::SkyEmu, FrontendSettings::WindowIcon::Runpog,
		};
		int iconIndex = 0;
		for (int i = 0; i < (int)std::size(iconValues); i++) {
			if (cfg.frontendSettings.icon == iconValues[i]) {
				iconIndex = i;
				break;
			}
		}
		if (ImGui::Combo("Window Icon", &iconIndex, iconLabels, (int)std::size(iconLabels))) {
			cfg.frontendSettings.icon = iconValues[iconIndex];
			reloadSettings = true;
		}

		bool showDebugPanel = cfg.frontendSettings.showImGuiDebugPanel;
		if (ImGui::Checkbox("Show ImGui Debug Panel", &showDebugPanel)) {
			cfg.frontendSettings.showImGuiDebugPanel = showDebugPanel;
			showDebug = showDebugPanel;
		}
	}

	if (ImGui::CollapsingHeader("Graphics")) {
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

		static const char* layoutLabels[] = {"Default", "Default Flipped", "Side By Side", "Side By Side Flipped"};
		static const ScreenLayout::Layout layoutValues[] = {
			ScreenLayout::Layout::Default, ScreenLayout::Layout::DefaultFlipped, ScreenLayout::Layout::SideBySide,
			ScreenLayout::Layout::SideBySideFlipped,
		};
		int layoutIndex = 0;
		for (int i = 0; i < (int)std::size(layoutValues); i++) {
			if (cfg.screenLayout == layoutValues[i]) {
				layoutIndex = i;
				break;
			}
		}
		if (ImGui::Combo("Screen Layout", &layoutIndex, layoutLabels, (int)std::size(layoutLabels))) {
			cfg.screenLayout = layoutValues[layoutIndex];
			reloadSettings = true;
		}
		reloadSettings |= ImGui::SliderFloat("Top Screen Size", &cfg.topScreenSize, 0.0f, 1.0f);
	}

	if (ImGui::CollapsingHeader("Audio")) {
		reloadSettings |= ImGui::Checkbox("Enable Audio", &cfg.audioEnabled);
		reloadSettings |= ImGui::Checkbox("Mute Audio", &cfg.audioDeviceConfig.muteAudio);
		reloadSettings |= ImGui::SliderFloat("Volume", &cfg.audioDeviceConfig.volumeRaw, 0.0f, 2.0f);
		reloadSettings |= ImGui::Checkbox("Enable AAC Audio", &cfg.aacEnabled);
		reloadSettings |= ImGui::Checkbox("Print DSP Firmware", &cfg.printDSPFirmware);

		static const char* dspLabels[] = {"Null", "Teakra", "HLE"};
		static const Audio::DSPCore::Type dspValues[] = {Audio::DSPCore::Type::Null, Audio::DSPCore::Type::Teakra, Audio::DSPCore::Type::HLE};
		int dspIndex = 0;
		for (int i = 0; i < (int)std::size(dspValues); i++) {
			if (cfg.dspType == dspValues[i]) {
				dspIndex = i;
				break;
			}
		}
		if (ImGui::Combo("DSP Emulation", &dspIndex, dspLabels, (int)std::size(dspLabels))) {
			cfg.dspType = dspValues[dspIndex];
			reloadSettings = true;
		}

		static const char* curveLabels[] = {"Cubic", "Linear"};
		static const AudioDeviceConfig::VolumeCurve curveValues[] = {AudioDeviceConfig::VolumeCurve::Cubic, AudioDeviceConfig::VolumeCurve::Linear};
		int curveIndex = 0;
		for (int i = 0; i < (int)std::size(curveValues); i++) {
			if (cfg.audioDeviceConfig.volumeCurve == curveValues[i]) {
				curveIndex = i;
				break;
			}
		}
		if (ImGui::Combo("Volume Curve", &curveIndex, curveLabels, (int)std::size(curveLabels))) {
			cfg.audioDeviceConfig.volumeCurve = curveValues[curveIndex];
			reloadSettings = true;
		}
	}

	if (ImGui::CollapsingHeader("Battery")) {
		reloadSettings |= ImGui::Checkbox("Charger Plugged", &cfg.chargerPlugged);
		reloadSettings |= ImGui::SliderInt("Battery %", &cfg.batteryPercentage, 0, 100);
	}

	if (ImGui::CollapsingHeader("SD")) {
		reloadSettings |= ImGui::Checkbox("Use Virtual SD", &cfg.sdCardInserted);
		reloadSettings |= ImGui::Checkbox("Write Protect SD", &cfg.sdWriteProtected);
	}

	ImGui::Separator();
	if (ImGui::Button("Save")) {
		saveConfig = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Close")) {
		showSettings = false;
	}

	ImGui::End();

	if (reloadSettings) {
		emu.reloadSettings();
	}
	if (saveConfig) {
		cfg.save();
	}
}

#endif
