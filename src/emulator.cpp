#include "emulator.hpp"
#include <glad/gl.h>

#ifdef _WIN32
#include <windows.h>

// Gently ask to use the discrete Nvidia/AMD GPU if possible instead of integrated graphics
extern "C" {
__declspec(dllexport) DWORD NvOptimusEnablement = 1;
__declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 1;
}
#endif

Emulator::Emulator()
	: config(std::filesystem::current_path() / "config.toml"), kernel(cpu, memory, gpu, config), cpu(memory, kernel), gpu(memory, config),
	  memory(cpu.getTicksRef(), config), cheats(memory, kernel.getServiceManager().getHID()), running(false), programRunning(false)
#ifdef PANDA3DS_ENABLE_HTTP_SERVER
	  , httpServer(this)
#endif
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
		Helpers::panic("Failed to initialize SDL2");
	}

	// Make SDL use consistent positional button mapping
	SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
	if (SDL_Init(SDL_INIT_GAMECONTROLLER) < 0) {
		Helpers::warn("Failed to initialize SDL2 GameController: %s", SDL_GetError());
	}

	// We need OpenGL for software rendering or for OpenGL if it's enabled
	bool needOpenGL = config.rendererType == RendererType::Software;
#ifdef PANDA3DS_ENABLE_OPENGL
	needOpenGL = needOpenGL || (config.rendererType == RendererType::OpenGL);
#endif

#ifdef PANDA3DS_ENABLE_DISCORD_RPC
	if (config.discordRpcEnabled) {
		discordRpc.init();
		updateDiscord();
	}
#endif

	if (needOpenGL) {
		// Demand 3.3 core for software renderer, or 4.1 core for OpenGL renderer (max available on MacOS)
		// MacOS gets mad if we don't explicitly demand a core profile
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, config.rendererType == RendererType::Software ? 3 : 4);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, config.rendererType == RendererType::Software ? 3 : 1);
		window = SDL_CreateWindow("Alber", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_OPENGL);

		if (window == nullptr) {
			Helpers::panic("Window creation failed: %s", SDL_GetError());
		}

		glContext = SDL_GL_CreateContext(window);
		if (glContext == nullptr) {
			Helpers::panic("OpenGL context creation failed: %s", SDL_GetError());
		}

		if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress))) {
			Helpers::panic("OpenGL init failed: %s", SDL_GetError());
		}
	}

#ifdef PANDA3DS_ENABLE_VULKAN
	if (config.rendererType == RendererType::Vulkan) {
		window = SDL_CreateWindow("Alber", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_VULKAN);

		if (window == nullptr) {
			Helpers::warn("Window creation failed: %s", SDL_GetError());
		}
	}
#endif

	if (SDL_WasInit(SDL_INIT_GAMECONTROLLER)) {
		gameController = SDL_GameControllerOpen(0);

		if (gameController != nullptr) {
			SDL_Joystick* stick = SDL_GameControllerGetJoystick(gameController);
			gameControllerID = SDL_JoystickInstanceID(stick);
		}
	}

	reset(ReloadOption::NoReload);
}

Emulator::~Emulator() {
	config.save(std::filesystem::current_path() / "config.toml");

#ifdef PANDA3DS_ENABLE_DISCORD_RPC
	discordRpc.stop();
#endif
}

void Emulator::reset(ReloadOption reload) {
	cpu.reset();
	gpu.reset();
	memory.reset();
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

void Emulator::step() {}
void Emulator::render() {}

void Emulator::run() {
	programRunning = true;

	while (programRunning) {
#ifdef PANDA3DS_ENABLE_HTTP_SERVER
		httpServer.processActions();
#endif

		runFrame();
		HIDService& hid = kernel.getServiceManager().getHID();

		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			namespace Keys = HID::Keys;

			switch (event.type) {
				case SDL_QUIT:
					printf("Bye :(\n");
					programRunning = false;
					return;

				case SDL_KEYDOWN:
					if (romType == ROMType::None) break;

					switch (event.key.keysym.sym) {
						case SDLK_l: hid.pressKey(Keys::A); break;
						case SDLK_k: hid.pressKey(Keys::B); break;
						case SDLK_o: hid.pressKey(Keys::X); break;
						case SDLK_i: hid.pressKey(Keys::Y); break;

						case SDLK_q: hid.pressKey(Keys::L); break;
						case SDLK_p: hid.pressKey(Keys::R); break;

						case SDLK_RIGHT: hid.pressKey(Keys::Right); break;
						case SDLK_LEFT: hid.pressKey(Keys::Left); break;
						case SDLK_UP: hid.pressKey(Keys::Up); break;
						case SDLK_DOWN: hid.pressKey(Keys::Down); break;

						case SDLK_w:
							hid.setCirclepadY(0x9C);
							keyboardAnalogY = true;
							break;

						case SDLK_a:
							hid.setCirclepadX(-0x9C);
							keyboardAnalogX = true;
							break;

						case SDLK_s:
							hid.setCirclepadY(-0x9C);
							keyboardAnalogY = true;
							break;

						case SDLK_d:
							hid.setCirclepadX(0x9C);
							keyboardAnalogX = true;
							break;

						case SDLK_RETURN: hid.pressKey(Keys::Start); break;
						case SDLK_BACKSPACE: hid.pressKey(Keys::Select); break;

						// Use the F4 button as a hot-key to pause or resume the emulator
						// We can't use the audio play/pause buttons because it's annoying 
						case SDLK_F4: {
							togglePause();
							break;
						}

						// Use F5 as a reset button
						case SDLK_F5: {
							reset(ReloadOption::Reload);
							break;
						}
					}
					break;

				case SDL_KEYUP:
					if (romType == ROMType::None) break;

					switch (event.key.keysym.sym) {
						case SDLK_l: hid.releaseKey(Keys::A); break;
						case SDLK_k: hid.releaseKey(Keys::B); break;
						case SDLK_o: hid.releaseKey(Keys::X); break;
						case SDLK_i: hid.releaseKey(Keys::Y); break;

						case SDLK_q: hid.releaseKey(Keys::L); break;
						case SDLK_p: hid.releaseKey(Keys::R); break;

						case SDLK_RIGHT: hid.releaseKey(Keys::Right); break;
						case SDLK_LEFT: hid.releaseKey(Keys::Left); break;
						case SDLK_UP: hid.releaseKey(Keys::Up); break;
						case SDLK_DOWN: hid.releaseKey(Keys::Down); break;

						// Err this is probably not ideal
						case SDLK_w:
						case SDLK_s:
							hid.setCirclepadY(0);
							keyboardAnalogY = false;
							break;

						case SDLK_a:
						case SDLK_d:
							hid.setCirclepadX(0);
							keyboardAnalogX = false;
							break;

						case SDLK_RETURN: hid.releaseKey(Keys::Start); break;
						case SDLK_BACKSPACE: hid.releaseKey(Keys::Select); break;
					}
					break;

				case SDL_MOUSEBUTTONDOWN:
					if (romType == ROMType::None) break;

					if (event.button.button == SDL_BUTTON_LEFT) {
						const s32 x = event.button.x;
						const s32 y = event.button.y;

						// Check if touch falls in the touch screen area
						if (y >= 240 && y <= 480 && x >= 40 && x < 40 + 320) {
							// Convert to 3DS coordinates
							u16 x_converted = static_cast<u16>(x) - 40;
							u16 y_converted = static_cast<u16>(y) - 240;

							hid.setTouchScreenPress(x_converted, y_converted);
						} else {
							hid.releaseTouchScreen();
						}
					} else if (event.button.button == SDL_BUTTON_RIGHT) {
						holdingRightClick = true;
					}

					break;

				case SDL_MOUSEBUTTONUP:
					if (romType == ROMType::None) break;

					if (event.button.button == SDL_BUTTON_LEFT) {
						hid.releaseTouchScreen();
					} else if (event.button.button == SDL_BUTTON_RIGHT) {
						holdingRightClick = false;
					}
					break;

				case SDL_CONTROLLERDEVICEADDED:
					if (gameController == nullptr) {
						gameController = SDL_GameControllerOpen(event.cdevice.which);
						gameControllerID = event.cdevice.which;
					}
					break;

				case SDL_CONTROLLERDEVICEREMOVED:
					if (event.cdevice.which == gameControllerID) {
						SDL_GameControllerClose(gameController);
						gameController = nullptr;
						gameControllerID = 0;
					}
					break;

				case SDL_CONTROLLERBUTTONUP:
				case SDL_CONTROLLERBUTTONDOWN: {
					if (romType == ROMType::None) break;
					u32 key = 0;

					switch (event.cbutton.button) {
						case SDL_CONTROLLER_BUTTON_A: key = Keys::B; break;
						case SDL_CONTROLLER_BUTTON_B: key = Keys::A; break;
						case SDL_CONTROLLER_BUTTON_X: key = Keys::Y; break;
						case SDL_CONTROLLER_BUTTON_Y: key = Keys::X; break;
						case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: key = Keys::L; break;
						case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: key = Keys::R; break;
						case SDL_CONTROLLER_BUTTON_DPAD_LEFT: key = Keys::Left; break;
						case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: key = Keys::Right; break;
						case SDL_CONTROLLER_BUTTON_DPAD_UP: key = Keys::Up; break;
						case SDL_CONTROLLER_BUTTON_DPAD_DOWN: key = Keys::Down; break;
						case SDL_CONTROLLER_BUTTON_BACK: key = Keys::Select; break;
						case SDL_CONTROLLER_BUTTON_START: key = Keys::Start; break;
					}

					if (key != 0) {
						if (event.cbutton.state == SDL_PRESSED) {
							hid.pressKey(key);
						} else {
							hid.releaseKey(key);
						}
					}
					break;
				}

				// Detect mouse motion events for gyroscope emulation
				case SDL_MOUSEMOTION: {
					if (romType == ROMType::None) break;

					// Handle "dragging" across the touchscreen
					if (hid.isTouchScreenPressed()) {
						const s32 x = event.motion.x;
						const s32 y = event.motion.y;

						// Check if touch falls in the touch screen area and register the new touch screen position
						if (y >= 240 && y <= 480 && x >= 40 && x < 40 + 320) {
							// Convert to 3DS coordinates
							u16 x_converted = static_cast<u16>(x) - 40;
							u16 y_converted = static_cast<u16>(y) - 240;

							hid.setTouchScreenPress(x_converted, y_converted);
						}
					}

					// We use right click to indicate we want to rotate the console. If right click is not held, then this is not a gyroscope rotation
					if (holdingRightClick) {
						// Relative motion since last mouse motion event
						const s32 motionX = event.motion.xrel;
						const s32 motionY = event.motion.yrel;

						// The gyroscope involves lots of weird math I don't want to bother with atm
						// So up until then, we will set the gyroscope euler angles to fixed values based on the direction of the relative motion
						const s32 roll = motionX > 0 ? 0x7f : -0x7f;
						const s32 pitch = motionY > 0 ? 0x7f : -0x7f;
						hid.setRoll(roll);
						hid.setPitch(pitch);
					}
					break;
				}

				case SDL_DROPFILE: {
					char* droppedDir = event.drop.file;

					if (droppedDir) {
						loadROM(droppedDir);
						SDL_free(droppedDir);
					}
					break;
				}
			}
		}

		// Update controller analog sticks and HID service
		if (romType != ROMType::None) {
			if (gameController != nullptr) {
				const s16 stickX = SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_LEFTX);
				const s16 stickY = SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_LEFTY);
				constexpr s16 deadzone = 3276;
				constexpr s16 maxValue = 0x9C;
				constexpr s16 div = 0x8000 / maxValue;

				// Avoid overriding the keyboard's circlepad input
				if (abs(stickX) < deadzone && !keyboardAnalogX) {
					hid.setCirclepadX(0);
				} else {
					hid.setCirclepadX(stickX / div);
				}

				if (abs(stickY) < deadzone && !keyboardAnalogY) {
					hid.setCirclepadY(0);
				} else {
					hid.setCirclepadY(-(stickY / div));
				}
			}

			hid.updateInputs(cpu.getTicks());
		}
		// TODO: Should this be uncommented?
		// kernel.evalReschedule();

		// Update inputs in the HID module
		SDL_GL_SwapWindow(window);
	}
}

// Only resume if a ROM is properly loaded
void Emulator::resume() { running = (romType != ROMType::None); }
void Emulator::pause() { running = false; }
void Emulator::togglePause() { running ? pause() : resume(); }

void Emulator::runFrame() {
	if (running) {
		cpu.runFrame(); // Run 1 frame of instructions
		gpu.display();  // Display graphics

		// Send VBlank interrupts
		ServiceManager& srv = kernel.getServiceManager();
		srv.sendGPUInterrupt(GPUInterrupt::VBlank0);
		srv.sendGPUInterrupt(GPUInterrupt::VBlank1);

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

bool Emulator::loadROM(const std::filesystem::path& path) {
	// Reset the emulator if we've already loaded a ROM
	if (romType != ROMType::None) {
		reset(ReloadOption::NoReload);
	}

	// Reset whatever state needs to be reset before loading a new ROM
	memory.loadedCXI = std::nullopt;
	memory.loaded3DSX = std::nullopt;

	// Get path for saving files (AppData on Windows, /home/user/.local/share/ApplcationName on Linux, etc)
	// Inside that path, we be use a game-specific folder as well. Eg if we were loading a ROM called PenguinDemo.3ds, the savedata would be in
	// %APPDATA%/Alber/PenguinDemo/SaveData on Windows, and so on. We do this because games save data in their own filesystem on the cart
	char* appData = SDL_GetPrefPath(nullptr, "Alber");
	const std::filesystem::path appDataPath = std::filesystem::path(appData);
	const std::filesystem::path dataPath = appDataPath / path.filename().stem();
	const std::filesystem::path aesKeysPath = appDataPath / "sysdata" / "aes_keys.txt";
	IOFile::setAppDataDir(dataPath);
	SDL_free(appData);

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

// Reset our graphics context and initialize the GPU's graphics context
void Emulator::initGraphicsContext() { gpu.initGraphicsContext(window); }

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