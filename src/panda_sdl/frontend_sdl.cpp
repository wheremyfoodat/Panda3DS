#include "panda_sdl/frontend_sdl.hpp"

#include <glad/gl.h>

FrontendSDL::FrontendSDL() : keyboardMappings(InputMappings::defaultKeyboardMappings()) {
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
		Helpers::panic("Failed to initialize SDL2");
	}

	// Make SDL use consistent positional button mapping
	SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
	if (SDL_Init(SDL_INIT_GAMECONTROLLER) < 0) {
		Helpers::warn("Failed to initialize SDL2 GameController: %s", SDL_GetError());
	}

	if (SDL_WasInit(SDL_INIT_GAMECONTROLLER)) {
		gameController = SDL_GameControllerOpen(0);

		if (gameController != nullptr) {
			SDL_Joystick* stick = SDL_GameControllerGetJoystick(gameController);
			gameControllerID = SDL_JoystickInstanceID(stick);
		}
	}

	const EmulatorConfig& config = emu.getConfig();
	// We need OpenGL for software rendering or for OpenGL if it's enabled
	bool needOpenGL = config.rendererType == RendererType::Software;
#ifdef PANDA3DS_ENABLE_OPENGL
	needOpenGL = needOpenGL || (config.rendererType == RendererType::OpenGL);
#endif

	if (needOpenGL) {
		// Demand 3.3 core for software renderer, or 4.1 core for OpenGL renderer (max available on MacOS)
		// MacOS gets mad if we don't explicitly demand a core profile
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, config.rendererType == RendererType::Software ? 3 : 4);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, config.rendererType == RendererType::Software ? 3 : 1);
		window = SDL_CreateWindow("Alber", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 400, 480, SDL_WINDOW_OPENGL);

		if (window == nullptr) {
			Helpers::panic("Window creation failed: %s", SDL_GetError());
		}

		glContext = SDL_GL_CreateContext(window);
		if (glContext == nullptr) {
			Helpers::panic("OpenGL context creation failed: %s", SDL_GetError());
		}

		if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress))) {
			Helpers::panic("OpenGL init failed");
		}

		SDL_GL_SetSwapInterval(config.vsyncEnabled ? 1 : 0);
	}

#ifdef PANDA3DS_ENABLE_VULKAN
	if (config.rendererType == RendererType::Vulkan) {
		window = SDL_CreateWindow("Alber", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 400, 480, SDL_WINDOW_VULKAN);

		if (window == nullptr) {
			Helpers::warn("Window creation failed: %s", SDL_GetError());
		}
	}
#endif

	emu.initGraphicsContext(window);
}

bool FrontendSDL::loadROM(const std::filesystem::path& path) { return emu.loadROM(path); }

void FrontendSDL::run() {
	programRunning = true;
	keyboardAnalogX = false;
	keyboardAnalogY = false;
	holdingRightClick = false;

	while (programRunning) {
#ifdef PANDA3DS_ENABLE_HTTP_SERVER
		httpServer.processActions();
#endif

		emu.runFrame();
		HIDService& hid = emu.getServiceManager().getHID();

		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			namespace Keys = HID::Keys;

			switch (event.type) {
				case SDL_QUIT:
					printf("Bye :(\n");
					programRunning = false;
					return;

				case SDL_KEYDOWN: {
					if (emu.romType == ROMType::None) break;

					u32 key = getMapping(event.key.keysym.sym);
					if (key != HID::Keys::Null) {
						switch (key) {
							case HID::Keys::CirclePadRight:
								hid.setCirclepadX(0x9C);
								keyboardAnalogX = true;
								break;
							case HID::Keys::CirclePadLeft:
								hid.setCirclepadX(-0x9C);
								keyboardAnalogX = true;
								break;
							case HID::Keys::CirclePadUp:
								hid.setCirclepadY(0x9C);
								keyboardAnalogY = true;
								break;
							case HID::Keys::CirclePadDown:
								hid.setCirclepadY(-0x9C);
								keyboardAnalogY = true;
								break;
							default: hid.pressKey(key); break;
						}
					} else {
						switch (event.key.keysym.sym) {
							// Use the F4 button as a hot-key to pause or resume the emulator
							// We can't use the audio play/pause buttons because it's annoying
							case SDLK_F4: {
								emu.togglePause();
								break;
							}

							// Use F5 as a reset button
							case SDLK_F5: {
								emu.reset(Emulator::ReloadOption::Reload);
								break;
							}
						}
					}
					break;
				}

				case SDL_KEYUP: {
					if (emu.romType == ROMType::None) break;

					u32 key = getMapping(event.key.keysym.sym);
					if (key != HID::Keys::Null) {
						switch (key) {
							// Err this is probably not ideal
							case HID::Keys::CirclePadRight:
							case HID::Keys::CirclePadLeft:
								hid.setCirclepadX(0);
								keyboardAnalogX = false;
								break;
							case HID::Keys::CirclePadUp:
							case HID::Keys::CirclePadDown:
								hid.setCirclepadY(0);
								keyboardAnalogY = false;
								break;
							default: hid.releaseKey(key); break;
						}
					}
					break;
				}

				case SDL_MOUSEBUTTONDOWN:
					if (emu.romType == ROMType::None) break;

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
					if (emu.romType == ROMType::None) break;

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
					if (emu.romType == ROMType::None) break;
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
					if (emu.romType == ROMType::None) break;

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
						const std::filesystem::path path(droppedDir);

						if (path.extension() == ".amiibo") {
							emu.loadAmiibo(path);
						} else if (path.extension() == ".lua") {
							emu.getLua().loadFile(droppedDir);
						} else {
							loadROM(path);
						}

						SDL_free(droppedDir);
					}
					break;
				}
			}
		}

		// Update controller analog sticks and HID service
		if (emu.romType != ROMType::None) {
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

			hid.updateInputs(emu.getTicks());
		}
		// TODO: Should this be uncommented?
		// kernel.evalReschedule();

		SDL_GL_SwapWindow(window);
	}
}
