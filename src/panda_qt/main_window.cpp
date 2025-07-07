#include "panda_qt/main_window.hpp"

#include <QDesktopServices>
#include <QFileDialog>
#include <QString>
#include <cmath>
#include <cstdio>
#include <fstream>

#include "cheats.hpp"
#include "input_mappings.hpp"
#include "sdl_sensors.hpp"
#include "services/dsp.hpp"
#include "version.hpp"

MainWindow::MainWindow(QApplication* app, QWidget* parent) : QMainWindow(parent), keyboardMappings(InputMappings::defaultKeyboardMappings()) {
	emu = new Emulator();

	loadTranslation();
	setWindowTitle(tr("Alber"));

	// Enable drop events for loading ROMs
	setAcceptDrops(true);
	resize(800, 240 * 4);
	show();

	// We pass a callback to the screen widget that will be triggered every time we resize the screen
	screen = new ScreenWidget([this](u32 width, u32 height) { handleScreenResize(width, height); }, this);
	setCentralWidget(screen);

	appRunning = true;
	// Set our menu bar up
	menuBar = new QMenuBar(nullptr);

	// Create menu bar menus
	auto fileMenu = menuBar->addMenu(tr("File"));
	auto emulationMenu = menuBar->addMenu(tr("Emulation"));
	auto toolsMenu = menuBar->addMenu(tr("Tools"));
	auto aboutMenu = menuBar->addMenu(tr("About"));

	// Create and bind actions for them
	auto loadGameAction = fileMenu->addAction(tr("Load game"));
	auto loadLuaAction = fileMenu->addAction(tr("Load Lua script"));
	auto openAppFolderAction = fileMenu->addAction(tr("Open Panda3DS folder"));

	connect(loadGameAction, &QAction::triggered, this, &MainWindow::selectROM);
	connect(loadLuaAction, &QAction::triggered, this, &MainWindow::selectLuaFile);
	connect(openAppFolderAction, &QAction::triggered, this, [this]() {
		QString path = QString::fromStdU16String(emu->getAppDataRoot().u16string());
		QDesktopServices::openUrl(QUrl::fromLocalFile(path));
	});

	auto pauseAction = emulationMenu->addAction(tr("Pause"));
	auto resumeAction = emulationMenu->addAction(tr("Resume"));
	auto resetAction = emulationMenu->addAction(tr("Reset"));
	auto configureAction = emulationMenu->addAction(tr("Configure"));
	configureAction->setMenuRole(QAction::PreferencesRole);

	connect(pauseAction, &QAction::triggered, this, [this]() { sendMessage(EmulatorMessage{.type = MessageType::Pause}); });
	connect(resumeAction, &QAction::triggered, this, [this]() { sendMessage(EmulatorMessage{.type = MessageType::Resume}); });
	connect(resetAction, &QAction::triggered, this, [this]() { sendMessage(EmulatorMessage{.type = MessageType::Reset}); });
	connect(configureAction, &QAction::triggered, this, [this]() { configWindow->show(); });

	auto dumpRomFSAction = toolsMenu->addAction(tr("Dump RomFS"));
	auto luaEditorAction = toolsMenu->addAction(tr("Open Lua Editor"));
	auto cheatsEditorAction = toolsMenu->addAction(tr("Open Cheats Editor"));
	auto patchWindowAction = toolsMenu->addAction(tr("Open Patch Window"));
	auto shaderEditorAction = toolsMenu->addAction(tr("Open Shader Editor"));
	auto cpuDebuggerAction = toolsMenu->addAction(tr("Open CPU Debugger"));
	auto threadDebuggerAction = toolsMenu->addAction(tr("Open Thread Debugger"));
	auto dumpDspFirmware = toolsMenu->addAction(tr("Dump loaded DSP firmware"));

	connect(dumpRomFSAction, &QAction::triggered, this, &MainWindow::dumpRomFS);
	connect(luaEditorAction, &QAction::triggered, this, [this]() { luaEditor->show(); });
	connect(shaderEditorAction, &QAction::triggered, this, [this]() { shaderEditor->show(); });
	connect(cheatsEditorAction, &QAction::triggered, this, [this]() { cheatsEditor->show(); });
	connect(patchWindowAction, &QAction::triggered, this, [this]() { patchWindow->show(); });
	connect(cpuDebuggerAction, &QAction::triggered, this, [this]() { cpuDebugger->show(); });
	connect(threadDebuggerAction, &QAction::triggered, this, [this]() { threadDebugger->show(); });
	connect(dumpDspFirmware, &QAction::triggered, this, &MainWindow::dumpDspFirmware);

	connect(this, &MainWindow::emulatorPaused, this, [this]() { threadDebugger->update(); }, Qt::BlockingQueuedConnection);

	auto aboutAction = aboutMenu->addAction(tr("About Panda3DS"));
	aboutAction->setMenuRole(QAction::AboutRole);
	connect(aboutAction, &QAction::triggered, this, &MainWindow::showAboutMenu);

	setMenuBar(menuBar);

	emu->setOutputSize(screen->surfaceWidth, screen->surfaceHeight);

	// Set up misc objects
	aboutWindow = new AboutWindow(nullptr);
	cheatsEditor = new CheatsWindow(emu, {}, this);
	patchWindow = new PatchWindow(this);
	luaEditor = new TextEditorWindow(this, "script.lua", "");
	shaderEditor = new ShaderEditorWindow(this, "shader.glsl", "");
	threadDebugger = new ThreadDebugger(emu, this);
	cpuDebugger = new CPUDebugger(emu, this);

	shaderEditor->setEnable(emu->getRenderer()->supportsShaderReload());
	if (shaderEditor->supported) {
		shaderEditor->setText(emu->getRenderer()->getUbershader());
	}

	configWindow = new ConfigWindow(
		[&]() {
			EmulatorMessage message{.type = MessageType::UpdateConfig};
			sendMessage(message);
		},
		[&]() { return this; }, emu->getConfig(), this
	);

	auto args = QCoreApplication::arguments();
	if (args.size() > 1) {
		auto romPath = std::filesystem::current_path() / args.at(1).toStdU16String();
		if (!emu->loadROM(romPath)) {
			// For some reason just .c_str() doesn't show the proper path
			Helpers::warn("Failed to load ROM file: %s", romPath.string().c_str());
		}
	}

	// Handle UI configs before setting up the emulator thread
	{
		auto& config = emu->getConfig();
		auto& windowSettings = config.windowSettings;

		if (windowSettings.rememberPosition) {
			setGeometry(windowSettings.x, windowSettings.y, windowSettings.width, config.windowSettings.height);
		}

		if (config.printAppVersion) {
			printf("Welcome to Panda3DS v%s!\n", PANDA3DS_VERSION);
		}

		screen->reloadScreenLayout(config.screenLayout, config.topScreenSize);
	}

	// The emulator graphics context for the thread should be initialized in the emulator thread due to how GL contexts work
	emuThread = std::thread([this]() {
		const RendererType rendererType = emu->getConfig().rendererType;
		usingGL = (rendererType == RendererType::OpenGL || rendererType == RendererType::Software || rendererType == RendererType::Null);
		usingVk = (rendererType == RendererType::Vulkan);
		usingMtl = (rendererType == RendererType::Metal);

		if (usingGL) {
			// Make GL context current for this thread, enable VSync
			GL::Context* glContext = screen->getGLContext();
			glContext->MakeCurrent();
			glContext->SetSwapInterval(emu->getConfig().vsyncEnabled ? 1 : 0);

			if (glContext->IsGLES()) {
				emu->getRenderer()->setupGLES();
			}

			emu->initGraphicsContext(glContext);
		} else if (usingVk) {
			Helpers::panic("Vulkan on Qt is currently WIP, try the SDL frontend instead!");
		} else if (usingMtl) {
			Helpers::panic("Metal on Qt currently doesn't work, try the SDL frontend instead!");
		} else {
			Helpers::panic("Unsupported graphics backend for Qt frontend!");
		}

		// We have to initialize controllers on the same thread they'll be polled in
		initControllers();
		emuThreadMainLoop();
	});
}

void MainWindow::emuThreadMainLoop() {
	while (appRunning) {
		{
			std::unique_lock lock(messageQueueMutex);

			// Dispatch all messages in the message queue
			if (!messageQueue.empty()) {
				for (const auto& msg : messageQueue) {
					dispatchMessage(msg);
				}

				messageQueue.clear();
			}
		}

		emu->runFrame();
		pollControllers();

		if (emu->romType != ROMType::None) {
			emu->getServiceManager().getHID().updateInputs(emu->getTicks());
		}

		swapEmuBuffer();
	}

	// Unbind GL context if we're using GL, otherwise some setups seem to be unable to join this thread
	if (usingGL) {
		screen->getGLContext()->DoneCurrent();
	}
}

void MainWindow::swapEmuBuffer() {
	if (usingGL) {
		screen->getGLContext()->SwapBuffers();
	} else {
		Helpers::panic("[Qt] Don't know how to swap buffers for the current rendering backend :(");
	}
}

void MainWindow::selectROM() {
	auto path = QFileDialog::getOpenFileName(
		this, tr("Select 3DS ROM to load"), QString::fromStdU16String(emu->getConfig().defaultRomPath.u16string()),
		tr("Nintendo 3DS ROMs (*.3ds *.cci *.cxi *.app *.ncch *.3dsx *.elf *.axf)")
	);

	if (!path.isEmpty()) {
		std::filesystem::path* p = new std::filesystem::path(path.toStdU16String());

		EmulatorMessage message{.type = MessageType::LoadROM};
		message.path.p = p;
		sendMessage(message);
	}
}

void MainWindow::selectLuaFile() {
	auto path = QFileDialog::getOpenFileName(this, tr("Select Lua script to load"), "", tr("Lua scripts (*.lua *.txt)"));

	if (!path.isEmpty()) {
		std::ifstream file(std::filesystem::path(path.toStdU16String()), std::ios::in);

		if (file.fail()) {
			printf("Failed to load selected lua file\n");
			return;
		}

		// Read whole file into an std::string string
		// Get file size, preallocate std::string to avoid furthermemory allocations
		std::string code;
		file.seekg(0, std::ios::end);
		code.resize(file.tellg());

		// Rewind and read the whole file
		file.seekg(0, std::ios::beg);
		file.read(&code[0], code.size());
		file.close();

		loadLuaScript(code);
		// Copy the Lua script to the Lua editor
		luaEditor->setText(code);
	}
}

// Stop emulator thread when the main window closes
void MainWindow::closeEvent(QCloseEvent* event) {
	appRunning = false;  // Set our running atomic to false in order to make the emulator thread stop, and join it

	if (emuThread.joinable()) {
		emuThread.join();
	}

	// Cache window position/size in config file to restore next time
	const QRect& windowGeometry = geometry();
	auto& windowConfig = emu->getConfig().windowSettings;

	windowConfig.x = windowGeometry.x();
	windowConfig.y = windowGeometry.y();
	windowConfig.width = windowGeometry.width();
	windowConfig.height = windowGeometry.height();
}

// Cleanup when the main window closes
MainWindow::~MainWindow() {
	delete emu;
	delete menuBar;
	delete aboutWindow;
	delete configWindow;
	delete cheatsEditor;
	delete luaEditor;
}

// Send a message to the emulator thread. Lock the mutex and just push back to the vector.
void MainWindow::sendMessage(const EmulatorMessage& message) {
	std::unique_lock lock(messageQueueMutex);
	messageQueue.push_back(message);
}

void MainWindow::dumpRomFS() {
	auto folder = QFileDialog::getExistingDirectory(
		this, tr("Select folder to dump RomFS files to"), "", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
	);

	if (folder.isEmpty()) {
		return;
	}
	std::filesystem::path path(folder.toStdU16String());

	messageQueueMutex.lock();
	RomFS::DumpingResult res = emu->dumpRomFS(path);
	messageQueueMutex.unlock();

	switch (res) {
		case RomFS::DumpingResult::Success: break;  // Yay!
		case RomFS::DumpingResult::InvalidFormat: {
			QMessageBox messageBox(
				QMessageBox::Icon::Warning, tr("Invalid format for RomFS dumping"),
				tr("The currently loaded app is not in a format that supports RomFS")
			);

			QAbstractButton* button = messageBox.addButton(tr("OK"), QMessageBox::ButtonRole::YesRole);
			button->setIcon(QIcon(":/docs/img/rsob_icon.png"));
			messageBox.exec();
			break;
		}

		case RomFS::DumpingResult::NoRomFS:
			QMessageBox::warning(this, tr("No RomFS found"), tr("No RomFS partition was found in the loaded app"));
			break;
	}
}

void MainWindow::dumpDspFirmware() {
	auto file = QFileDialog::getSaveFileName(this, tr("Select file"), "", tr("DSP firmware file (*.cdc)"));

	if (file.isEmpty()) {
		return;
	}
	std::filesystem::path path(file.toStdU16String());

	messageQueueMutex.lock();
	auto res = emu->getServiceManager().getDSP().dumpComponent(path);
	messageQueueMutex.unlock();

	switch (res) {
		case DSPService::ComponentDumpResult::Success: break;
		case DSPService::ComponentDumpResult::NotLoaded: {
			QMessageBox messageBox(
				QMessageBox::Icon::Warning, tr("No DSP firmware loaded"), tr("The currently loaded app has not uploaded a firmware to the DSP")
			);

			QAbstractButton* button = messageBox.addButton(tr("OK"), QMessageBox::ButtonRole::YesRole);
			button->setIcon(QIcon(":/docs/img/rsob_icon.png"));
			messageBox.exec();
			break;
		}

		case DSPService::ComponentDumpResult::FileFailure: {
			QMessageBox messageBox(
				QMessageBox::Icon::Warning, tr("Failed to open output file"),
				tr("The currently loaded DSP firmware could not be written to the selected file. Please make sure you have permission to access this "
				   "file")
			);

			QAbstractButton* button = messageBox.addButton(tr("OK"), QMessageBox::ButtonRole::YesRole);
			button->setIcon(QIcon(":/docs/img/rstarstruck_icon.png"));
			messageBox.exec();
			break;
		}
	}
}

void MainWindow::showAboutMenu() {
	AboutWindow about(this);
	about.exec();
}

void MainWindow::dispatchMessage(const EmulatorMessage& message) {
	switch (message.type) {
		case MessageType::LoadROM:
			emu->loadROM(*message.path.p);
			// Clean up the allocated path
			delete message.path.p;
			break;

		case MessageType::LoadLuaScript:
			emu->getLua().loadString(*message.string.str);
			delete message.string.str;
			break;

		case MessageType::EditCheat: {
			u32 handle = message.cheat.c->handle;
			const std::vector<uint8_t>& cheat = message.cheat.c->cheat;
			const std::function<void(u32)>& callback = message.cheat.c->callback;
			bool isEditing = handle != Cheats::badCheatHandle;
			if (isEditing) {
				emu->getCheats().removeCheat(handle);
				u32 handle = emu->getCheats().addCheat(cheat.data(), cheat.size());
			} else {
				u32 handle = emu->getCheats().addCheat(cheat.data(), cheat.size());
				callback(handle);
			}
			delete message.cheat.c;
		} break;

		case MessageType::Resume: emu->resume(); break;
		case MessageType::Pause:
			emu->pause();
			emit emulatorPaused();
			break;

		case MessageType::TogglePause:
			emu->togglePause();
			if (!emu->running) {
				emit emulatorPaused();
			};
			break;

		case MessageType::Reset: emu->reset(Emulator::ReloadOption::Reload); break;
		case MessageType::PressKey: emu->getServiceManager().getHID().pressKey(message.key.key); break;
		case MessageType::ReleaseKey: emu->getServiceManager().getHID().releaseKey(message.key.key); break;

		// Track whether we're controlling the analog stick with our controller and update the CirclePad X/Y values in HID
		// Controllers are polled on the emulator thread, so this message type is only used when the circlepad is changed via keyboard input
		case MessageType::SetCirclePadX: {
			keyboardAnalogX = message.circlepad.value != 0;
			emu->getServiceManager().getHID().setCirclepadX(message.circlepad.value);
			break;
		}

		case MessageType::SetCirclePadY: {
			keyboardAnalogY = message.circlepad.value != 0;
			emu->getServiceManager().getHID().setCirclepadY(message.circlepad.value);
			break;
		}

		case MessageType::PressTouchscreen:
			emu->getServiceManager().getHID().setTouchScreenPress(message.touchscreen.x, message.touchscreen.y);
			break;
		case MessageType::ReleaseTouchscreen: emu->getServiceManager().getHID().releaseTouchScreen(); break;

		case MessageType::ReloadUbershader:
			emu->getRenderer()->setUbershader(*message.string.str);
			delete message.string.str;
			break;

		case MessageType::SetScreenSize: {
			const u32 width = message.screenSize.width;
			const u32 height = message.screenSize.height;
			screen->resizeSurface(width, height);
			emu->setOutputSize(width, height);
			break;
		}

		case MessageType::UpdateConfig: {
			auto& emuConfig = emu->getConfig();
			auto& newConfig = configWindow->getConfig();
			// If the screen layout changed, we have to notify the emulator & the screen widget
			bool reloadScreenLayout = (emuConfig.screenLayout != newConfig.screenLayout || emuConfig.topScreenSize != newConfig.topScreenSize);

			emuConfig = newConfig;
			emu->reloadSettings();

			if (reloadScreenLayout) {
				emu->reloadScreenLayout();
				screen->reloadScreenLayout(newConfig.screenLayout, newConfig.topScreenSize);
			}

			// Save new settings to disk
			emuConfig.save();
			break;
		}
	}
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
	auto pressKey = [this](u32 key) {
		EmulatorMessage message{.type = MessageType::PressKey};
		message.key.key = key;
		sendMessage(message);
	};

	auto setCirclePad = [this](MessageType type, s16 value) {
		EmulatorMessage message{.type = type};
		message.circlepad.value = value;
		sendMessage(message);
	};

	u32 key = keyboardMappings.getMapping(event->key());
	if (key != HID::Keys::Null) {
		switch (key) {
			case HID::Keys::CirclePadUp: setCirclePad(MessageType::SetCirclePadY, 0x9C); break;
			case HID::Keys::CirclePadDown: setCirclePad(MessageType::SetCirclePadY, -0x9C); break;
			case HID::Keys::CirclePadLeft: setCirclePad(MessageType::SetCirclePadX, -0x9C); break;
			case HID::Keys::CirclePadRight: setCirclePad(MessageType::SetCirclePadX, 0x9C); break;

			default: pressKey(key); break;
		}
	} else {
		switch (event->key()) {
			case Qt::Key_F4: sendMessage(EmulatorMessage{.type = MessageType::TogglePause}); break;
			case Qt::Key_F5: sendMessage(EmulatorMessage{.type = MessageType::Reset}); break;
		}
	}
}

void MainWindow::keyReleaseEvent(QKeyEvent* event) {
	auto releaseKey = [this](u32 key) {
		EmulatorMessage message{.type = MessageType::ReleaseKey};
		message.key.key = key;
		sendMessage(message);
	};

	auto releaseCirclePad = [this](MessageType type) {
		EmulatorMessage message{.type = type};
		message.circlepad.value = 0;
		sendMessage(message);
	};

	u32 key = keyboardMappings.getMapping(event->key());
	if (key != HID::Keys::Null) {
		switch (key) {
			case HID::Keys::CirclePadUp: releaseCirclePad(MessageType::SetCirclePadY); break;
			case HID::Keys::CirclePadDown: releaseCirclePad(MessageType::SetCirclePadY); break;
			case HID::Keys::CirclePadLeft: releaseCirclePad(MessageType::SetCirclePadX); break;
			case HID::Keys::CirclePadRight: releaseCirclePad(MessageType::SetCirclePadX); break;

			default: releaseKey(key); break;
		}
	}
}

void MainWindow::mousePressEvent(QMouseEvent* event) {
	if (event->button() == Qt::MouseButton::LeftButton) {
		// We handle actual mouse press & movement logic inside the mouseMoveEvent handler
		handleTouchscreenPress(event);
	}
}

void MainWindow::mouseMoveEvent(QMouseEvent* event) {
	if (event->buttons().testFlag(Qt::MouseButton::LeftButton)) {
		handleTouchscreenPress(event);
	}
}

void MainWindow::mouseReleaseEvent(QMouseEvent* event) {
	if (event->button() == Qt::MouseButton::LeftButton) {
		sendMessage(EmulatorMessage{.type = MessageType::ReleaseTouchscreen});
	}
}

void MainWindow::handleTouchscreenPress(QMouseEvent* event) {
	const QPointF clickPos = event->globalPosition();
	const QPointF widgetPos = screen->mapFromGlobal(clickPos);

	const auto& coords = screen->screenCoordinates;
	const float bottomScreenX = float(coords.bottomScreenX);
	const float bottomScreenY = float(coords.bottomScreenY);
	const float bottomScreenWidth = float(coords.bottomScreenWidth);
	const float bottomScreenHeight = float(coords.bottomScreenHeight);

	// Press is inside the screen area
	if (widgetPos.x() >= bottomScreenX && widgetPos.x() < bottomScreenX + bottomScreenWidth && widgetPos.y() >= bottomScreenY &&
		widgetPos.y() < bottomScreenY + bottomScreenHeight) {
		// Map widget position to 3DS touchscreen coordinates, with (0, 0) = top left of touchscreen
		float relX = (widgetPos.x() - bottomScreenX) / bottomScreenWidth;
		float relY = (widgetPos.y() - bottomScreenY) / bottomScreenHeight;

		u16 x_converted = u16(std::clamp(relX * ScreenLayout::BOTTOM_SCREEN_WIDTH, 0.f, float(ScreenLayout::BOTTOM_SCREEN_WIDTH - 1)));
		u16 y_converted = u16(std::clamp(relY * ScreenLayout::BOTTOM_SCREEN_HEIGHT, 0.f, float(ScreenLayout::BOTTOM_SCREEN_HEIGHT - 1)));

		EmulatorMessage message{.type = MessageType::PressTouchscreen};
		message.touchscreen.x = x_converted;
		message.touchscreen.y = y_converted;
		sendMessage(message);
	} else {
		sendMessage(EmulatorMessage{.type = MessageType::ReleaseTouchscreen});
	}
}

void MainWindow::loadLuaScript(const std::string& code) {
	EmulatorMessage message{.type = MessageType::LoadLuaScript};

	// Make a copy of the code on the heap to send via the message queue
	message.string.str = new std::string(code);
	sendMessage(message);
}

void MainWindow::reloadShader(const std::string& shader) {
	EmulatorMessage message{.type = MessageType::ReloadUbershader};

	// Make a copy of the code on the heap to send via the message queue
	message.string.str = new std::string(shader);
	sendMessage(message);
}

void MainWindow::editCheat(u32 handle, const std::vector<uint8_t>& cheat, const std::function<void(u32)>& callback) {
	EmulatorMessage message{.type = MessageType::EditCheat};

	CheatMessage* c = new CheatMessage();
	c->handle = handle;
	c->cheat = cheat;
	c->callback = callback;

	message.cheat.c = c;
	sendMessage(message);
}

void MainWindow::handleScreenResize(u32 width, u32 height) {
	EmulatorMessage message{.type = MessageType::SetScreenSize};
	message.screenSize.width = width;
	message.screenSize.height = height;

	if (messageQueueMutex.try_lock()) {
		messageQueue.push_back(message);
		messageQueueMutex.unlock();
	}
}

void MainWindow::initControllers() {
	// Make SDL use consistent positional button mapping
	SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
	if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC) < 0) {
		Helpers::warn("Failed to initialize SDL2 GameController: %s", SDL_GetError());
		return;
	}

	if (SDL_WasInit(SDL_INIT_GAMECONTROLLER)) {
		gameController = SDL_GameControllerOpen(0);

		if (gameController != nullptr) {
			SDL_Joystick* stick = SDL_GameControllerGetJoystick(gameController);
			gameControllerID = SDL_JoystickInstanceID(stick);
		}

		setupControllerSensors(gameController);
	}
}

void MainWindow::pollControllers() {
	// Update circlepad/c-stick/ZL/ZR if a controller is plugged in
	if (gameController != nullptr) {
		HIDService& hid = emu->getServiceManager().getHID();
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

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		HIDService& hid = emu->getServiceManager().getHID();
		using namespace HID;

		switch (event.type) {
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
				if (emu->romType == ROMType::None) break;
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
		}
	}
}

void MainWindow::setupControllerSensors(SDL_GameController* controller) {
	bool haveGyro = SDL_GameControllerHasSensor(controller, SDL_SENSOR_GYRO) == SDL_TRUE;
	bool haveAccelerometer = SDL_GameControllerHasSensor(controller, SDL_SENSOR_ACCEL) == SDL_TRUE;

	if (haveGyro) {
		SDL_GameControllerSetSensorEnabled(controller, SDL_SENSOR_GYRO, SDL_TRUE);
	}

	if (haveAccelerometer) {
		SDL_GameControllerSetSensorEnabled(controller, SDL_SENSOR_ACCEL, SDL_TRUE);
	}
}
