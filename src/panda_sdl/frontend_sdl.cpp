#include "panda_sdl/frontend_sdl.hpp"

#include <glad/gl.h>

#include "renderdoc.hpp"
#include "sdl_sensors.hpp"
#include "version.hpp"

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

		setupControllerSensors(gameController);
	}

	const EmulatorConfig& config = emu.getConfig();
	// We need OpenGL for software rendering/null renderer or for the OpenGL renderer if it's enabled.
	bool needOpenGL = config.rendererType == RendererType::Software || config.rendererType == RendererType::Null;
#ifdef PANDA3DS_ENABLE_OPENGL
	needOpenGL = needOpenGL || (config.rendererType == RendererType::OpenGL);
#endif

	const char* windowTitle = config.windowSettings.showAppVersion ? ("Alber v" PANDA3DS_VERSION) : "Alber";
	if (config.printAppVersion) {
		printf("Welcome to Panda3DS v%s!\n", PANDA3DS_VERSION);
	}

	// Positions of the window
	int windowX, windowY;

	// Apply window size settings if the appropriate option is enabled
	if (config.windowSettings.rememberPosition) {
		windowX = config.windowSettings.x;
		windowY = config.windowSettings.y;
		windowWidth = config.windowSettings.width;
		windowHeight = config.windowSettings.height;
	} else {
		windowX = SDL_WINDOWPOS_CENTERED;
		windowY = SDL_WINDOWPOS_CENTERED;
		windowWidth = 400;
		windowHeight = 480;
	}

	// Initialize output size and screen layout
	emu.setOutputSize(windowWidth, windowHeight);
	ScreenLayout::calculateCoordinates(
		screenCoordinates, u32(windowWidth), u32(windowHeight), emu.getConfig().topScreenSize, emu.getConfig().screenLayout
	);

	if (needOpenGL) {
		// Demand 4.1 core for OpenGL renderer (max available on MacOS), 3.3 for the software & null renderers
		// MacOS gets mad if we don't explicitly demand a core profile
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, config.rendererType == RendererType::OpenGL ? 4 : 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, config.rendererType == RendererType::OpenGL ? 1 : 3);
		window = SDL_CreateWindow(windowTitle, windowX, windowY, windowWidth, windowHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

		if (window == nullptr) {
			Helpers::panic("Window creation failed: %s", SDL_GetError());
		}

		glContext = SDL_GL_CreateContext(window);
		if (glContext == nullptr) {
			Helpers::warn("OpenGL context creation failed: %s\nTrying again with OpenGL ES.", SDL_GetError());

			// Some low end devices (eg RPi, emulation handhelds) don't support desktop GL, but only OpenGL ES, so fall back to that if GL context
			// creation failed
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
			glContext = SDL_GL_CreateContext(window);
			if (glContext == nullptr) {
				Helpers::panic("OpenGL context creation failed: %s", SDL_GetError());
			}

			if (!gladLoadGLES2Loader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress))) {
				Helpers::panic("OpenGL init failed");
			}

			emu.getRenderer()->setupGLES();
		} else {
			if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress))) {
				Helpers::panic("OpenGL init failed");
			}
		}

		SDL_GL_SetSwapInterval(config.vsyncEnabled ? 1 : 0);
	}

#ifdef PANDA3DS_ENABLE_VULKAN
	if (config.rendererType == RendererType::Vulkan) {
		window = SDL_CreateWindow(windowTitle, windowX, windowY, windowWidth, windowHeight, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

		if (window == nullptr) {
			Helpers::warn("Window creation failed: %s", SDL_GetError());
		}
	}
#endif

#ifdef PANDA3DS_ENABLE_METAL
	if (config.rendererType == RendererType::Metal) {
		window = SDL_CreateWindow(windowTitle, windowX, windowY, windowWidth, windowHeight, SDL_WINDOW_METAL | SDL_WINDOW_RESIZABLE);

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
				case SDL_QUIT: {
					printf("Bye :(\n");
					programRunning = false;

					// Remember window position & size for future runs
					auto& windowSettings = emu.getConfig().windowSettings;
					SDL_GetWindowPosition(window, &windowSettings.x, &windowSettings.y);
					SDL_GetWindowSize(window, &windowSettings.width, &windowSettings.height);
					return;
				}

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

							case SDLK_F11: {
								if constexpr (Renderdoc::isSupported()) {
									Renderdoc::triggerCapture();
								}

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
						handleLeftClick(event.button.x, event.button.y);
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

						setupControllerSensors(gameController);
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
						if (windowWidth == 0 || windowHeight == 0) [[unlikely]] {
							break;
						}

						handleLeftClick(event.motion.x, event.motion.y);
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

				case SDL_CONTROLLERSENSORUPDATE: {
					if (event.csensor.sensor == SDL_SENSOR_GYRO) {
						auto rotation = Sensors::SDL::convertRotation({
							event.csensor.data[0],
							event.csensor.data[1],
							event.csensor.data[2],
						});

						hid.setPitch(s16(rotation.x));
						hid.setRoll(s16(rotation.y));
						hid.setYaw(s16(rotation.z));
					} else if (event.csensor.sensor == SDL_SENSOR_ACCEL) {
						auto accel = Sensors::SDL::convertAcceleration(event.csensor.data);
						hid.setAccel(accel.x, accel.y, accel.z);
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

				case SDL_WINDOWEVENT: {
					auto type = event.window.event;
					if (type == SDL_WINDOWEVENT_RESIZED) {
						windowWidth = event.window.data1;
						windowHeight = event.window.data2;

						const auto& config = emu.getConfig();
						ScreenLayout::calculateCoordinates(
							screenCoordinates, u32(windowWidth), u32(windowHeight), emu.getConfig().topScreenSize, emu.getConfig().screenLayout
						);

						emu.setOutputSize(windowWidth, windowHeight);
					}
				}
			}
		}

		// Update controller analog sticks and HID service
		if (emu.romType != ROMType::None) {
			// Update circlepad/c-stick/ZL/ZR if a controller is plugged in
			if (gameController != nullptr) {
				const s16 stickX = SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_LEFTX);
				const s16 stickY = SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_LEFTY);
				constexpr s16 deadzone = 3276;
				constexpr s16 triggerThreshold = SDL_JOYSTICK_AXIS_MAX / 2;

				{
					// Update circlepad
					constexpr s16 circlepadMax = 0x9C;
					constexpr s16 div = 0x8000 / circlepadMax;

					// Avoid overriding the keyboard's circlepad input
					if (std::abs(stickX) < deadzone && !keyboardAnalogX) {
						hid.setCirclepadX(0);
					} else {
						hid.setCirclepadX(stickX / div);
					}

					if (std::abs(stickY) < deadzone && !keyboardAnalogY) {
						hid.setCirclepadY(0);
					} else {
						hid.setCirclepadY(-(stickY / div));
					}
				}

				const s16 l2 = SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
				const s16 r2 = SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
				const s16 cStickX = SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_RIGHTX);
				const s16 cStickY = SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_RIGHTY);

				hid.setKey(HID::Keys::ZL, l2 > triggerThreshold);
				hid.setKey(HID::Keys::ZR, r2 > triggerThreshold);

				// Update C-Stick
				// To convert from SDL coordinates, ie [-32768, 32767] to [-2048, 2047] we just divide by 8
				if (std::abs(cStickX) < deadzone) {
					hid.setCStickX(IR::CirclePadPro::ButtonState::C_STICK_CENTER);
				} else {
					hid.setCStickX(cStickX / 8);
				}

				if (std::abs(cStickY) < deadzone) {
					hid.setCStickY(IR::CirclePadPro::ButtonState::C_STICK_CENTER);
				} else {
					hid.setCStickY(-(cStickY / 8));
				}
			}

			hid.updateInputs(emu.getTicks());
		}
		// TODO: Should this be uncommented?
		// kernel.evalReschedule();

		SDL_GL_SwapWindow(window);
	}
}

void FrontendSDL::setupControllerSensors(SDL_GameController* controller) {
	bool haveGyro = SDL_GameControllerHasSensor(controller, SDL_SENSOR_GYRO) == SDL_TRUE;
	bool haveAccelerometer = SDL_GameControllerHasSensor(controller, SDL_SENSOR_ACCEL) == SDL_TRUE;

	if (haveGyro) {
		SDL_GameControllerSetSensorEnabled(controller, SDL_SENSOR_GYRO, SDL_TRUE);
	}

	if (haveAccelerometer) {
		SDL_GameControllerSetSensorEnabled(controller, SDL_SENSOR_ACCEL, SDL_TRUE);
	}
}

void FrontendSDL::handleLeftClick(int mouseX, int mouseY) {
	if (windowWidth == 0 || windowHeight == 0) [[unlikely]] {
		return;
	}

	const auto& coords = screenCoordinates;
	const int bottomScreenX = int(coords.bottomScreenX);
	const int bottomScreenY = int(coords.bottomScreenY);
	const int bottomScreenWidth = int(coords.bottomScreenWidth);
	const int bottomScreenHeight = int(coords.bottomScreenHeight);
	auto& hid = emu.getServiceManager().getHID();

	// Check if the mouse is inside the bottom screen area
	if (mouseX >= int(bottomScreenX) && mouseX < int(bottomScreenX + bottomScreenWidth) && mouseY >= int(bottomScreenY) &&
		mouseY < int(bottomScreenY + bottomScreenHeight)) {
		// Map to 3DS touchscreen coordinates
		float relX = float(mouseX - bottomScreenX) / float(bottomScreenWidth);
		float relY = float(mouseY - bottomScreenY) / float(bottomScreenHeight);

		u16 x_converted = static_cast<u16>(std::clamp(relX * ScreenLayout::BOTTOM_SCREEN_WIDTH, 0.f, float(ScreenLayout::BOTTOM_SCREEN_WIDTH - 1)));
		u16 y_converted = static_cast<u16>(std::clamp(relY * ScreenLayout::BOTTOM_SCREEN_HEIGHT, 0.f, float(ScreenLayout::BOTTOM_SCREEN_HEIGHT - 1)));

		hid.setTouchScreenPress(x_converted, y_converted);
	} else {
		hid.releaseTouchScreen();
	}
}