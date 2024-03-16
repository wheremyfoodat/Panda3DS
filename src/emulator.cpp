#include "emulator.hpp"

#ifndef __ANDROID__
#include <SDL_filesystem.h>
#endif

#include <fstream>

#ifdef _WIN32
#include <windows.h>

// Gently ask to use the discrete Nvidia/AMD GPU if possible instead of integrated graphics
extern "C" {
__declspec(dllexport) DWORD NvOptimusEnablement = 1;
__declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 1;
}
#endif

Emulator::Emulator()
	: config(getConfigPath()), kernel(cpu, memory, gpu, config), cpu(memory, kernel, *this), gpu(memory, config), memory(cpu.getTicksRef(), config),
	  cheats(memory, kernel.getServiceManager().getHID()), lua(*this), running(false)
#ifdef PANDA3DS_ENABLE_HTTP_SERVER
	  ,
	  httpServer(this)
#endif
{
	DSPService& dspService = kernel.getServiceManager().getDSP();

	dsp = Audio::makeDSPCore(config.dspType, memory, scheduler, dspService);
	dspService.setDSPCore(dsp.get());

	audioDevice.init(dsp->getSamples());
	setAudioEnabled(config.audioEnabled);

#ifdef PANDA3DS_ENABLE_DISCORD_RPC
	if (config.discordRpcEnabled) {
		discordRpc.init();
		updateDiscord();
	}
#endif
	reset(ReloadOption::NoReload);
}

Emulator::~Emulator() {
	config.save();
	lua.close();

#ifdef PANDA3DS_ENABLE_DISCORD_RPC
	discordRpc.stop();
#endif
}

void Emulator::reset(ReloadOption reload) {
	cpu.reset();
	gpu.reset();
	memory.reset();
	dsp->reset();

	// Reset scheduler and add a VBlank event
	scheduler.reset();

	// Kernel must be reset last because it depends on CPU/Memory state
	kernel.reset();

	// Reloading r13 and r15 needs to happen after everything has been reset
	// Otherwise resetting the kernel or cpu might nuke them
	cpu.setReg(13, VirtualAddrs::StackTop);  // Set initial SP

	// We're resetting without reloading the ROM, so yeet cheats
	if (reload == ReloadOption::NoReload) {
		cheats.reset();
	}

	// If a ROM is active and we reset, with the reload option enabled then reload it.
	// This is necessary to set up stack, executable memory, .data/.rodata/.bss all over again
	if (reload == ReloadOption::Reload && romType != ROMType::None && romPath.has_value()) {
		bool success = loadROM(romPath.value());
		if (!success) {
			romType = ROMType::None;
			romPath = std::nullopt;

			Helpers::panic("Failed to reload ROM. This should pause the emulator in the future GUI");
		}
	}
}

std::filesystem::path Emulator::getAndroidAppPath() {
	// SDL_GetPrefPath fails to get the path due to no JNI environment
	std::ifstream cmdline("/proc/self/cmdline");
	std::string applicationName;
	std::getline(cmdline, applicationName, '\0');

	return std::filesystem::path("/data") / "data" / applicationName / "files";
}

std::filesystem::path Emulator::getConfigPath() {
	if constexpr (Helpers::isAndroid()) {
		return getAndroidAppPath() / "config.toml";
	} else {
		return std::filesystem::current_path() / "config.toml";
	}
}

void Emulator::step() {}
void Emulator::render() {}

// Only resume if a ROM is properly loaded
void Emulator::resume() {
	running = (romType != ROMType::None);

	if (running && config.audioEnabled) {
		audioDevice.start();
	}
}

void Emulator::pause() {
	running = false;
	audioDevice.stop();
}

void Emulator::togglePause() { running ? pause() : resume(); }

void Emulator::runFrame() {
	if (running) {
		cpu.runFrame(); // Run 1 frame of instructions
		gpu.display();  // Display graphics

		// Run cheats if any are loaded
		if (cheats.haveCheats()) [[unlikely]] {
			cheats.run();
		}
	} else if (romType != ROMType::None) {
		// If the emulator is not running and a game is loaded, we still want to display the framebuffer otherwise we will get weird
		// double-buffering issues
		gpu.display();
	}
}

void Emulator::pollScheduler() {
	auto& events = scheduler.events;

	// Pop events until there's none pending anymore
	while (scheduler.currentTimestamp >= scheduler.nextTimestamp) {
		// Read event timestamp and type, pop it from the scheduler and handle it
		auto [time, eventType] = std::move(*events.begin());
		events.erase(events.begin());

		scheduler.updateNextTimestamp();

		switch (eventType) {
			case Scheduler::EventType::VBlank: [[likely]] {
				// Signal that we've reached the end of a frame
				frameDone = true;
				lua.signalEvent(LuaEvent::Frame);

				// Send VBlank interrupts
				ServiceManager& srv = kernel.getServiceManager();
				srv.sendGPUInterrupt(GPUInterrupt::VBlank0);
				srv.sendGPUInterrupt(GPUInterrupt::VBlank1);

				// Queue next VBlank event
				scheduler.addEvent(Scheduler::EventType::VBlank, time + CPU::ticksPerSec / 60);
				break;
			}

			case Scheduler::EventType::UpdateTimers: kernel.pollTimers(); break;
			case Scheduler::EventType::RunDSP: {
				dsp->runAudioFrame();
				break;
			}

			default: {
				Helpers::panic("Scheduler: Unimplemented event type received: %d\n", static_cast<int>(eventType));
				break;
			}
		}
	}
}

// Get path for saving files (AppData on Windows, /home/user/.local/share/ApplicationName on Linux, etc)
// Inside that path, we be use a game-specific folder as well. Eg if we were loading a ROM called PenguinDemo.3ds, the savedata would be in
// %APPDATA%/Alber/PenguinDemo/SaveData on Windows, and so on. We do this because games save data in their own filesystem on the cart.
// If the portable build setting is enabled, then those saves go in the executable directory instead
std::filesystem::path Emulator::getAppDataRoot() {
	std::filesystem::path appDataPath;

#ifdef __ANDROID__
	appDataPath = getAndroidAppPath();
#else
	char* appData;
	if (!config.usePortableBuild) {
		appData = SDL_GetPrefPath(nullptr, "Alber");
		appDataPath = std::filesystem::path(appData);
	} else {
		appData = SDL_GetBasePath();
		appDataPath = std::filesystem::path(appData) / "Emulator Files";
	}
	SDL_free(appData);
#endif

	return appDataPath;
}

bool Emulator::loadROM(const std::filesystem::path& path) {
	// Reset the emulator if we've already loaded a ROM
	if (romType != ROMType::None) {
		reset(ReloadOption::NoReload);
	}

	// Reset whatever state needs to be reset before loading a new ROM
	memory.loadedCXI = std::nullopt;
	memory.loaded3DSX = std::nullopt;

	const std::filesystem::path appDataPath = getAppDataRoot();
	const std::filesystem::path dataPath = appDataPath / path.filename().stem();
	const std::filesystem::path aesKeysPath = appDataPath / "sysdata" / "aes_keys.txt";
	IOFile::setAppDataDir(dataPath);

	// Open the text file containing our AES keys if it exists. We use the std::filesystem::exists overload that takes an error code param to
	// avoid the call throwing exceptions
	std::error_code ec;
	if (std::filesystem::exists(aesKeysPath, ec) && !ec) {
		aesEngine.loadKeys(aesKeysPath);
	}

	kernel.initializeFS();
	auto extension = path.extension();
	bool success;  // Tracks if we loaded the ROM successfully

	if (extension == ".elf" || extension == ".axf")
		success = loadELF(path);
	else if (extension == ".3ds" || extension == ".cci")
		success = loadNCSD(path, ROMType::NCSD);
	else if (extension == ".cxi" || extension == ".app")
		success = loadNCSD(path, ROMType::CXI);
	else if (extension == ".3dsx")
		success = load3DSX(path);
	else {
		printf("Unknown file type\n");
		success = false;
	}

	if (success) {
		romPath = path;
#ifdef PANDA3DS_ENABLE_DISCORD_RPC
		updateDiscord();
#endif
	} else {
		romPath = std::nullopt;
		romType = ROMType::None;
	}

	resume();  // Start the emulator
	return success;
}

bool Emulator::loadAmiibo(const std::filesystem::path& path) {
	NFCService& nfc = kernel.getServiceManager().getNFC();
	return nfc.loadAmiibo(path);
}

// Used for loading both CXI and NCSD files since they are both so similar and use the same interface
// (We promote CXI files to NCSD internally for ease)
bool Emulator::loadNCSD(const std::filesystem::path& path, ROMType type) {
	romType = type;
	std::optional<NCSD> opt = (type == ROMType::NCSD) ? memory.loadNCSD(aesEngine, path) : memory.loadCXI(aesEngine, path);

	if (!opt.has_value()) {
		return false;
	}

	loadedNCSD = opt.value();
	cpu.setReg(15, loadedNCSD.entrypoint);

	if (loadedNCSD.entrypoint & 1) {
		Helpers::panic("Misaligned NCSD entrypoint; should this start the CPU in Thumb mode?");
	}

	return true;
}

bool Emulator::load3DSX(const std::filesystem::path& path) {
	std::optional<u32> entrypoint = memory.load3DSX(path);
	romType = ROMType::HB_3DSX;

	if (!entrypoint.has_value()) {
		return false;
	}

	cpu.setReg(15, entrypoint.value());  // Set initial PC

	return true;
}

bool Emulator::loadELF(const std::filesystem::path& path) {
	loadedELF.open(path, std::ios_base::binary);  // Open ROM in binary mode
	romType = ROMType::ELF;

	return loadELF(loadedELF);
}

bool Emulator::loadELF(std::ifstream& file) {
	// Rewind ifstream
	loadedELF.clear();
	loadedELF.seekg(0);

	std::optional<u32> entrypoint = memory.loadELF(loadedELF);
	if (!entrypoint.has_value()) {
		return false;
	}

	cpu.setReg(15, entrypoint.value());  // Set initial PC
	if (entrypoint.value() & 1) {
		Helpers::panic("Misaligned ELF entrypoint. TODO: Check if ELFs can boot in thumb mode");
	}

	return true;
}

std::span<u8> Emulator::getSMDH() {
	switch (romType) {
		case ROMType::NCSD:
		case ROMType::CXI:
			return memory.getCXI()->smdh;
		default: {
			return std::span<u8>();
		}
	}
}

#ifdef PANDA3DS_ENABLE_DISCORD_RPC
void Emulator::updateDiscord() {
	if (config.discordRpcEnabled) {
		if (romType != ROMType::None) {
			const auto name = romPath.value().stem();
			discordRpc.update(Discord::RPCStatus::Playing, name.string());
		} else {
			discordRpc.update(Discord::RPCStatus::Idling, "");
		}
	}
}
#else
void Emulator::updateDiscord() {}
#endif

static void dumpRomFSNode(const RomFS::RomFSNode& node, const char* romFSBase, const std::filesystem::path& path) {
	for (auto& file : node.files) {
		const auto p = path / file->name;
		std::ofstream outFile(p);

		outFile.write(romFSBase + file->dataOffset, file->dataSize);
	}

	for (auto& directory : node.directories) {
		const auto newPath = path / directory->name;
		
		// Create the directory for the new folder
		std::error_code ec;
		std::filesystem::create_directories(newPath, ec);

		if (!ec) {
			dumpRomFSNode(*directory, romFSBase, newPath);
		}
	}
}

RomFS::DumpingResult Emulator::dumpRomFS(const std::filesystem::path& path) {
	using namespace RomFS;

	if (romType != ROMType::NCSD && romType != ROMType::CXI && romType != ROMType::HB_3DSX) {
		return DumpingResult::InvalidFormat;
	}

	// Contents of RomFS as raw bytes
	std::vector<u8> romFS;
	u64 size;

	if (romType == ROMType::HB_3DSX) {
		auto hb3dsx = memory.get3DSX();
		if (!hb3dsx->hasRomFs()) {
			return DumpingResult::NoRomFS;
		}
		size = hb3dsx->romFSSize;

		romFS.resize(size);
		hb3dsx->readRomFSBytes(&romFS[0], 0, size);
	} else {
		auto cxi = memory.getCXI();
		if (!cxi->hasRomFS()) {
			return DumpingResult::NoRomFS;
		}

		const u64 offset = cxi->romFS.offset;
		size = cxi->romFS.size;

		romFS.resize(size);
		cxi->readFromFile(memory.CXIFile, cxi->partitionInfo, &romFS[0], offset - cxi->fileOffset, size);
	}

	std::unique_ptr<RomFSNode> node = parseRomFSTree((uintptr_t)&romFS[0], size);
	dumpRomFSNode(*node, (const char*)&romFS[0], path);

	return DumpingResult::Success;
}

void Emulator::setAudioEnabled(bool enable) {
	if (!enable) {
		audioDevice.stop();
	} else if (enable && romType != ROMType::None && running) {
		// Don't start the audio device yet if there's no ROM loaded or the emulator is paused
		// Resume and Pause will handle it
		audioDevice.start();
	}

	dsp->setAudioEnabled(enable);
}
