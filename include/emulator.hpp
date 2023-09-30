#pragma once

#include <SDL.h>

#include <filesystem>
#include <fstream>
#include <optional>

#include "PICA/gpu.hpp"
#include "cheats.hpp"
#include "config.hpp"
#include "cpu.hpp"
#include "crypto/aes_engine.hpp"
#include "discord_rpc.hpp"
#include "io_file.hpp"
#include "lua_manager.hpp"
#include "memory.hpp"

#ifdef PANDA3DS_ENABLE_HTTP_SERVER
#include "http_server.hpp"
#endif

#ifdef PANDA3DS_FRONTEND_QT
#include "gl/context.h"
#endif

enum class ROMType {
	None,
	ELF,
	NCSD,
	CXI,
	HB_3DSX,
};

class Emulator {
	EmulatorConfig config;
	CPU cpu;
	GPU gpu;
	Memory memory;
	Kernel kernel;
	Crypto::AESEngine aesEngine;
	Cheats cheats;

#ifdef PANDA3DS_FRONTEND_SDL
	SDL_Window* window;

#ifdef PANDA3DS_ENABLE_OPENGL
	SDL_GLContext glContext;
#endif
#endif

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
	bool running = false;         // Is the emulator running a game?
	bool programRunning = false;  // Is the emulator program itself running?

#ifdef PANDA3DS_ENABLE_HTTP_SERVER
	HttpServer httpServer;
	friend struct HttpServer;
#endif

#ifdef PANDA3DS_ENABLE_DISCORD_RPC
	Discord::RPC discordRpc;
#endif
	void updateDiscord();

	// Keep the handle for the ROM here to reload when necessary and to prevent deleting it
	// This is currently only used for ELFs, NCSDs use the IOFile API instead
	std::ifstream loadedELF;
	NCSD loadedNCSD;

	std::optional<std::filesystem::path> romPath = std::nullopt;
	LuaManager lua;

  public:
	// Decides whether to reload or not reload the ROM when resetting. We use enum class over a plain bool for clarity.
	// If NoReload is selected, the emulator will not reload its selected ROM. This is useful for things like booting up the emulator, or resetting to
	// change ROMs. If Reload is selected, the emulator will reload its selected ROM. This is useful for eg a "reset" button that keeps the current
	// ROM and just resets the emu
	enum class ReloadOption { NoReload, Reload };

	Emulator();
	~Emulator();

	void step();
	void render();
	void reset(ReloadOption reload);
	void run();
	void runFrame();

	void resume();  // Resume the emulator
	void pause();   // Pause the emulator
	void togglePause();

	bool loadAmiibo(const std::filesystem::path& path);
	bool loadROM(const std::filesystem::path& path);
	bool loadNCSD(const std::filesystem::path& path, ROMType type);
	bool load3DSX(const std::filesystem::path& path);
	bool loadELF(const std::filesystem::path& path);
	bool loadELF(std::ifstream& file);

#ifdef PANDA3DS_FRONTEND_QT
	// For passing the GL context from Qt to the renderer
	void initGraphicsContext(GL::Context* glContext) { gpu.initGraphicsContext(nullptr); }
#else
	void initGraphicsContext() { gpu.initGraphicsContext(window); }
#endif

	EmulatorConfig& getConfig() { return config; }
};
