#include "panda_qt/main_window.hpp"

#include <QDesktopServices>
#include <QFileDialog>
#include <QString>
#include <cstdio>
#include <fstream>

#include "cheats.hpp"

MainWindow::MainWindow(QApplication* app, QWidget* parent) : QMainWindow(parent), screen(this) {
	setWindowTitle("Alber");
	// Enable drop events for loading ROMs
	setAcceptDrops(true);
	resize(800, 240 * 4);
	screen.show();

	appRunning = true;

	// Set our menu bar up
	menuBar = new QMenuBar(this);
	setMenuBar(menuBar);

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
	connect(pauseAction, &QAction::triggered, this, [this]() { sendMessage(EmulatorMessage{.type = MessageType::Pause}); });
	connect(resumeAction, &QAction::triggered, this, [this]() { sendMessage(EmulatorMessage{.type = MessageType::Resume}); });
	connect(resetAction, &QAction::triggered, this, [this]() { sendMessage(EmulatorMessage{.type = MessageType::Reset}); });
	connect(configureAction, &QAction::triggered, this, [this]() { configWindow->show(); });

	auto dumpRomFSAction = toolsMenu->addAction(tr("Dump RomFS"));
	auto luaEditorAction = toolsMenu->addAction(tr("Open Lua Editor"));
	auto cheatsEditorAction = toolsMenu->addAction(tr("Open Cheats Editor"));
	connect(dumpRomFSAction, &QAction::triggered, this, &MainWindow::dumpRomFS);
	connect(luaEditorAction, &QAction::triggered, this, &MainWindow::openLuaEditor);
	connect(cheatsEditorAction, &QAction::triggered, this, &MainWindow::openCheatsEditor);

	auto aboutAction = aboutMenu->addAction(tr("About Panda3DS"));
	connect(aboutAction, &QAction::triggered, this, &MainWindow::showAboutMenu);

	emu = new Emulator();
	emu->setOutputSize(screen.surfaceWidth, screen.surfaceHeight);

	// Set up misc objects
	aboutWindow = new AboutWindow(nullptr);
	configWindow = new ConfigWindow(this);
	cheatsEditor = new CheatsWindow(emu, {}, this);
	luaEditor = new TextEditorWindow(this, "script.lua", "");

	auto args = QCoreApplication::arguments();
	if (args.size() > 1) {
		auto romPath = std::filesystem::current_path() / args.at(1).toStdU16String();
		if (!emu->loadROM(romPath)) {
			// For some reason just .c_str() doesn't show the proper path
			Helpers::warn("Failed to load ROM file: %s", romPath.string().c_str());
		}
	}

	// The emulator graphics context for the thread should be initialized in the emulator thread due to how GL contexts work
	emuThread = std::thread([this]() {
		const RendererType rendererType = emu->getConfig().rendererType;
		usingGL = (rendererType == RendererType::OpenGL || rendererType == RendererType::Software || rendererType == RendererType::Null);
		usingVk = (rendererType == RendererType::Vulkan);

		if (usingGL) {
			// Make GL context current for this thread, enable VSync
			GL::Context* glContext = screen.getGLContext();
			glContext->MakeCurrent();
			glContext->SetSwapInterval(1);

			emu->initGraphicsContext(glContext);
		} else if (usingVk) {
			Helpers::panic("Vulkan on Qt is currently WIP, try the SDL frontend instead!");
		} else {
			Helpers::panic("Unsupported graphics backend for Qt frontend!");
		}

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
		if (emu->romType != ROMType::None) {
			emu->getServiceManager().getHID().updateInputs(emu->getTicks());
		}

		swapEmuBuffer();
	}

	// Unbind GL context if we're using GL, otherwise some setups seem to be unable to join this thread
	if (usingGL) {
		screen.getGLContext()->DoneCurrent();
	}
}

void MainWindow::swapEmuBuffer() {
	if (usingGL) {
		screen.getGLContext()->SwapBuffers();
	} else {
		Helpers::panic("[Qt] Don't know how to swap buffers for the current rendering backend :(");
	}
}

void MainWindow::selectROM() {
	auto path =
		QFileDialog::getOpenFileName(this, tr("Select 3DS ROM to load"), "", tr("Nintendo 3DS ROMs (*.3ds *.cci *.cxi *.app *.3dsx *.elf *.axf)"));

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

// Cleanup when the main window closes
MainWindow::~MainWindow() {
	appRunning = false;  // Set our running atomic to false in order to make the emulator thread stop, and join it

	if (emuThread.joinable()) {
		emuThread.join();
	}

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

void MainWindow::showAboutMenu() {
	AboutWindow about(this);
	about.exec();
}

void MainWindow::openLuaEditor() { luaEditor->show(); }

void MainWindow::openCheatsEditor() { cheatsEditor->show(); }

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
			bool isEditing = handle != badCheatHandle;
			if (isEditing) {
				emu->getCheats().removeCheat(handle);
				u32 handle = emu->getCheats().addCheat(cheat.data(), cheat.size());
			} else {
				u32 handle = emu->getCheats().addCheat(cheat.data(), cheat.size());
				callback(handle);
			}
			delete message.cheat.c;
		} break;

		case MessageType::Pause: emu->pause(); break;
		case MessageType::Resume: emu->resume(); break;
		case MessageType::TogglePause: emu->togglePause(); break;
		case MessageType::Reset: emu->reset(Emulator::ReloadOption::Reload); break;
		case MessageType::PressKey: emu->getServiceManager().getHID().pressKey(message.key.key); break;
		case MessageType::ReleaseKey: emu->getServiceManager().getHID().releaseKey(message.key.key); break;
	}
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
	auto pressKey = [this](u32 key) {
		EmulatorMessage message{.type = MessageType::PressKey};
		message.key.key = key;

		sendMessage(message);
	};

	switch (event->key()) {
		case Qt::Key_L: pressKey(HID::Keys::A); break;
		case Qt::Key_K: pressKey(HID::Keys::B); break;
		case Qt::Key_O: pressKey(HID::Keys::X); break;
		case Qt::Key_I: pressKey(HID::Keys::Y); break;

		case Qt::Key_Q: pressKey(HID::Keys::L); break;
		case Qt::Key_P: pressKey(HID::Keys::R); break;

		case Qt::Key_Right: pressKey(HID::Keys::Right); break;
		case Qt::Key_Left: pressKey(HID::Keys::Left); break;
		case Qt::Key_Up: pressKey(HID::Keys::Up); break;
		case Qt::Key_Down: pressKey(HID::Keys::Down); break;

		case Qt::Key_Return: pressKey(HID::Keys::Start); break;
		case Qt::Key_Backspace: pressKey(HID::Keys::Select); break;
		case Qt::Key_F4: sendMessage(EmulatorMessage{.type = MessageType::TogglePause}); break;
		case Qt::Key_F5: sendMessage(EmulatorMessage{.type = MessageType::Reset}); break;
	}
}

void MainWindow::keyReleaseEvent(QKeyEvent* event) {
	auto releaseKey = [this](u32 key) {
		EmulatorMessage message{.type = MessageType::ReleaseKey};
		message.key.key = key;

		sendMessage(message);
	};

	switch (event->key()) {
		case Qt::Key_L: releaseKey(HID::Keys::A); break;
		case Qt::Key_K: releaseKey(HID::Keys::B); break;
		case Qt::Key_O: releaseKey(HID::Keys::X); break;
		case Qt::Key_I: releaseKey(HID::Keys::Y); break;

		case Qt::Key_Q: releaseKey(HID::Keys::L); break;
		case Qt::Key_P: releaseKey(HID::Keys::R); break;

		case Qt::Key_Right: releaseKey(HID::Keys::Right); break;
		case Qt::Key_Left: releaseKey(HID::Keys::Left); break;
		case Qt::Key_Up: releaseKey(HID::Keys::Up); break;
		case Qt::Key_Down: releaseKey(HID::Keys::Down); break;

		case Qt::Key_Return: releaseKey(HID::Keys::Start); break;
		case Qt::Key_Backspace: releaseKey(HID::Keys::Select); break;
	}
}

void MainWindow::loadLuaScript(const std::string& code) {
	EmulatorMessage message{.type = MessageType::LoadLuaScript};

	// Make a copy of the code on the heap to send via the message queue
	message.string.str = new std::string(code);
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