#include "emulator.hpp"

#ifdef _WIN32
#include <windows.h>

// Gently ask to use the discrete Nvidia/AMD GPU if possible instead of integrated graphics
extern "C" {
_declspec(dllexport) DWORD NvOptimusEnablement = 1;
_declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 1;
}
#endif

Emulator::Emulator() : kernel(cpu, memory, gpu), cpu(memory, kernel), gpu(memory, gl), memory(cpu.getTicksRef()) {
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
		Helpers::panic("Failed to initialize SDL2");
	}

	// Make SDL use consistent positional button mapping
	SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
	if (SDL_Init(SDL_INIT_GAMECONTROLLER) < 0) {
		Helpers::warn("Failed to initialize SDL2 GameController: %s", SDL_GetError());
	}

	// Request OpenGL 4.1 Core (Max available on MacOS)
	// MacOS gets mad if we don't explicitly demand a core profile
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
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

	if (SDL_WasInit(SDL_INIT_GAMECONTROLLER)) {
		gameController = SDL_GameControllerOpen(0);

		if (gameController != nullptr) {
			SDL_Joystick* stick = SDL_GameControllerGetJoystick(gameController);
			gameControllerID = SDL_JoystickInstanceID(stick);
		}
	}

	reset();
}

void Emulator::reset() {
	cpu.reset();
	gpu.reset();
	memory.reset();
	// Kernel must be reset last because it depends on CPU/Memory state
	kernel.reset();

	// Reloading r13 and r15 needs to happen after everything has been reset
	// Otherwise resetting the kernel or cpu might nuke them
	cpu.setReg(13, VirtualAddrs::StackTop);  // Set initial SP
	if (romType == ROMType::ELF) {           // Reload ELF if we're using one
		loadELF(loadedELF);
	}
}

void Emulator::step() {}
void Emulator::render() {}

void Emulator::run() {
    while (running) {
        runFrame(); // Run 1 frame of instructions
        gpu.display(); // Display graphics

		ServiceManager& srv = kernel.getServiceManager();

		// Send VBlank interrupts
		srv.sendGPUInterrupt(GPUInterrupt::VBlank0);
		srv.sendGPUInterrupt(GPUInterrupt::VBlank1);

		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			namespace Keys = HID::Keys;

			switch (event.type) {
				case SDL_QUIT:
					printf("Bye :(\n");
					running = false;
					return;

				case SDL_KEYDOWN:
					switch (event.key.keysym.sym) {
						case SDLK_l: srv.pressKey(Keys::A); break;
						case SDLK_k: srv.pressKey(Keys::B); break;
						case SDLK_o: srv.pressKey(Keys::X); break;
						case SDLK_i: srv.pressKey(Keys::Y); break;

						case SDLK_q: srv.pressKey(Keys::L); break;
						case SDLK_p: srv.pressKey(Keys::R); break;

						case SDLK_RIGHT: srv.pressKey(Keys::Right); break;
						case SDLK_LEFT: srv.pressKey(Keys::Left); break;
						case SDLK_UP: srv.pressKey(Keys::Up); break;
						case SDLK_DOWN: srv.pressKey(Keys::Down); break;

						case SDLK_w:
							srv.setCirclepadY(0x9C);
							keyboardAnalogY = true;
							break;

						case SDLK_a:
							srv.setCirclepadX(-0x9C);
							keyboardAnalogX = true;
							break;

						case SDLK_s:
							srv.setCirclepadY(-0x9C);
							keyboardAnalogY = true;
							break;

						case SDLK_d:
							srv.setCirclepadX(0x9C);
							keyboardAnalogX = true;
							break;

						case SDLK_RETURN: srv.pressKey(Keys::Start); break;
						case SDLK_BACKSPACE: srv.pressKey(Keys::Select); break;
					}
					break;

				case SDL_KEYUP:
					switch (event.key.keysym.sym) {
						case SDLK_l: srv.releaseKey(Keys::A); break;
						case SDLK_k: srv.releaseKey(Keys::B); break;
						case SDLK_o: srv.releaseKey(Keys::X); break;
						case SDLK_i: srv.releaseKey(Keys::Y); break;

						case SDLK_q: srv.releaseKey(Keys::L); break;
						case SDLK_p: srv.releaseKey(Keys::R); break;

						case SDLK_RIGHT: srv.releaseKey(Keys::Right); break;
						case SDLK_LEFT: srv.releaseKey(Keys::Left); break;
						case SDLK_UP: srv.releaseKey(Keys::Up); break;
						case SDLK_DOWN: srv.releaseKey(Keys::Down); break;

						// Err this is probably not ideal
						case SDLK_w:
						case SDLK_s:
							srv.setCirclepadY(0);
							keyboardAnalogY = false;
							break;

						case SDLK_a:
						case SDLK_d:
							srv.setCirclepadX(0);
							keyboardAnalogX = false;
							break;

						case SDLK_RETURN: srv.releaseKey(Keys::Start); break;
						case SDLK_BACKSPACE: srv.releaseKey(Keys::Select); break;
					}
					break;

				case SDL_MOUSEBUTTONDOWN: {
					if (event.button.button == SDL_BUTTON_LEFT) {
						const s32 x = event.button.x;
						const s32 y = event.button.y;

						// Check if touch falls in the touch screen area
						if (y >= 240 && y <= 480 && x >= 40 && x < 40 + 320) {
							// Convert to 3DS coordinates
							u16 x_converted = static_cast<u16>(x) - 40;
							u16 y_converted = static_cast<u16>(y) - 240;

							srv.setTouchScreenPress(x_converted, y_converted);
						} else {
							srv.releaseTouchScreen();
						}
					}
					break;
				}

				case SDL_MOUSEBUTTONUP:
					if (event.button.button == SDL_BUTTON_LEFT) {
						srv.releaseTouchScreen();
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
							srv.pressKey(key);
						} else {
							srv.releaseKey(key);
						}
					}
				}
			}
		}

		if (gameController != nullptr) {
			const s16 stickX = SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_LEFTX);
			const s16 stickY = SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_LEFTY);
			constexpr s16 deadzone = 3276;
			constexpr s16 maxValue = 0x9C;
			constexpr s16 div = 0x8000 / maxValue;

			// Avoid overriding the keyboard's circlepad input
			if (abs(stickX) < deadzone && !keyboardAnalogX) {
				srv.setCirclepadX(0);
			} else {
				srv.setCirclepadX(stickX / div);
			}

			if (abs(stickY) < deadzone && !keyboardAnalogY) {
				srv.setCirclepadY(0);
			} else {
				srv.setCirclepadY(-(stickY / div));
			}
		}

		// Update inputs in the HID module
		srv.updateInputs(cpu.getTicks());
		SDL_GL_SwapWindow(window);
	}
}

void Emulator::runFrame() { cpu.runFrame(); }

bool Emulator::loadROM(const std::filesystem::path& path) {
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

	if (extension == ".elf" || extension == ".axf")
		return loadELF(path);
	else if (extension == ".3ds")
		return loadNCSD(path);
	else {
		printf("Unknown file type\n");
		return false;
	}
}

bool Emulator::loadNCSD(const std::filesystem::path& path) {
	romType = ROMType::NCSD;
	std::optional<NCSD> opt = memory.loadNCSD(aesEngine, path);

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
void Emulator::initGraphicsContext() {
	gl.reset(); // TODO (For when we have multiple backends): Only do this if we are using OpenGL
	gpu.initGraphicsContext();
}