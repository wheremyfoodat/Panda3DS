#pragma once

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <span>

#include "PICA/gpu.hpp"
#include "audio/dsp_core.hpp"
#include "audio/miniaudio_device.hpp"
#include "cheats.hpp"
#include "config.hpp"
#include "cpu.hpp"
#include "crypto/aes_engine.hpp"
#include "discord_rpc.hpp"
#include "fs/romfs.hpp"
#include "io_file.hpp"
#include "lua_manager.hpp"
#include "memory.hpp"
#include "scheduler.hpp"

#ifdef PANDA3DS_ENABLE_HTTP_SERVER
#include "http_server.hpp"
#endif

#ifdef PANDA3DS_FRONTEND_QT
#include "gl/context.h"
#endif

struct SDL_Window;

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
	std::unique_ptr<Audio::DSPCore> dsp;
	Scheduler scheduler;

	Crypto::AESEngine aesEngine;
	MiniAudioDevice audioDevice;
	Cheats cheats;

  public:
	static constexpr u32 width = 400;
	static constexpr u32 height = 240 * 2;  // * 2 because 2 screens
	ROMType romType = ROMType::None;
	bool running = false;         // Is the emulator running a game?

  private:
#ifdef PANDA3DS_ENABLE_HTTP_SERVER
	HttpServer httpServer;
	friend struct HttpServer;
#endif

#ifdef PANDA3DS_ENABLE_DISCORD_RPC
	Discord::RPC discordRpc;
#endif
	void setAudioEnabled(bool enable);
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
	// Used in CPU::runFrame
	bool frameDone = false;

	Emulator();
	~Emulator();

	void step();
	void render();
	void reset(ReloadOption reload);
	void runFrame();
	// Poll the scheduler for events
	void pollScheduler();

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
	void initGraphicsContext(SDL_Window* window) { gpu.initGraphicsContext(window); }
#endif

	RomFS::DumpingResult dumpRomFS(const std::filesystem::path& path);
	void setOutputSize(u32 width, u32 height) { gpu.setOutputSize(width, height); }
	void deinitGraphicsContext() { gpu.deinitGraphicsContext(); }

	EmulatorConfig& getConfig() { return config; }
	Cheats& getCheats() { return cheats; }
	ServiceManager& getServiceManager() { return kernel.getServiceManager(); }
	LuaManager& getLua() { return lua; }
	Scheduler& getScheduler() { return scheduler; }
	Memory& getMemory() { return memory; }

	RendererType getRendererType() const { return config.rendererType; }
	Renderer* getRenderer() { return gpu.getRenderer(); }
	u64 getTicks() { return cpu.getTicks(); }

	std::filesystem::path getConfigPath();
	std::filesystem::path getAndroidAppPath();
	// Get the root path for the emulator's app data
	std::filesystem::path getAppDataRoot();

	std::span<u8> getSMDH();
};
