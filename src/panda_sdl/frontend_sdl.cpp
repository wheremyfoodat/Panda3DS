#include "panda_sdl/frontend_sdl.hpp"

#include <glad/gl.h>

FrontendSDL::FrontendSDL() {
	if (SDL_WasInit(SDL_INIT_GAMECONTROLLER)) {
		gameController = SDL_GameControllerOpen(0);

		if (gameController != nullptr) {
			SDL_Joystick* stick = SDL_GameControllerGetJoystick(gameController);
			gameControllerID = SDL_JoystickInstanceID(stick);
		}
	}
}

bool FrontendSDL::loadROM(const std::filesystem::path& path) { return emu.loadROM(path); }
void FrontendSDL::run() { emu.run(this); }

void Emulator::run(void* frontend) {
	FrontendSDL* frontendSDL = reinterpret_cast<FrontendSDL*>(frontend);
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
					if (frontendSDL->gameController == nullptr) {
						frontendSDL->gameController = SDL_GameControllerOpen(event.cdevice.which);
						frontendSDL->gameControllerID = event.cdevice.which;
					}
					break;

				case SDL_CONTROLLERDEVICEREMOVED:
					if (event.cdevice.which == frontendSDL->gameControllerID) {
						SDL_GameControllerClose(frontendSDL->gameController);
						frontendSDL->gameController = nullptr;
						frontendSDL->gameControllerID = 0;
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
						const std::filesystem::path path(droppedDir);

						if (path.extension() == ".amiibo") {
							loadAmiibo(path);
						} else if (path.extension() == ".lua") {
							lua.loadFile(droppedDir);
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
		if (romType != ROMType::None) {
			if (frontendSDL->gameController != nullptr) {
				const s16 stickX = SDL_GameControllerGetAxis(frontendSDL->gameController, SDL_CONTROLLER_AXIS_LEFTX);
				const s16 stickY = SDL_GameControllerGetAxis(frontendSDL->gameController, SDL_CONTROLLER_AXIS_LEFTY);
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

		window.swapBuffers();
	}
}
