#include "panda_qt/main_window.hpp"

#include <QFileDialog>
#include <cstdio>

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
	auto helpMenu = menuBar->addMenu(tr("Help"));
	auto aboutMenu = menuBar->addMenu(tr("About"));

	// Create and bind actions for them
	auto pandaAction = fileMenu->addAction(tr("panda..."));
	connect(pandaAction, &QAction::triggered, this, &MainWindow::selectROM);

	auto pauseAction = emulationMenu->addAction(tr("Pause"));
	auto resumeAction = emulationMenu->addAction(tr("Resume"));
	auto resetAction = emulationMenu->addAction(tr("Reset"));
	connect(pauseAction, &QAction::triggered, this, [this]() { sendMessage(EmulatorMessage{.type = MessageType::Pause}); });
	connect(resumeAction, &QAction::triggered, this, [this]() { sendMessage(EmulatorMessage{.type = MessageType::Resume}); });
	connect(resetAction, &QAction::triggered, this, [this]() { sendMessage(EmulatorMessage{.type = MessageType::Reset}); });

	auto dumpRomFSAction = toolsMenu->addAction(tr("Dump RomFS"));
	connect(dumpRomFSAction, &QAction::triggered, this, &MainWindow::dumpRomFS);

	// Set up theme selection
	setTheme(Theme::Dark);
	themeSelect = new QComboBox(this);
	themeSelect->addItem(tr("System"));
	themeSelect->addItem(tr("Light"));
	themeSelect->addItem(tr("Dark"));
	themeSelect->setCurrentIndex(static_cast<int>(currentTheme));

	themeSelect->setGeometry(40, 40, 100, 50);
	themeSelect->show();
	connect(themeSelect, &QComboBox::currentIndexChanged, this, [&](int index) { setTheme(static_cast<Theme>(index)); });

	emu = new Emulator();
	emu->setOutputSize(screen.surfaceWidth, screen.surfaceHeight);

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

// Cleanup when the main window closes
MainWindow::~MainWindow() {
	appRunning = false; // Set our running atomic to false in order to make the emulator thread stop, and join it
	
	if (emuThread.joinable()) {
		emuThread.join();
	}

	delete emu;
	delete menuBar;
	delete themeSelect;
}

// Send a message to the emulator thread. Lock the mutex and just push back to the vector.
void MainWindow::sendMessage(const EmulatorMessage& message) {
	std::unique_lock lock(messageQueueMutex);
	messageQueue.push_back(message);
}

void MainWindow::setTheme(Theme theme) {
	currentTheme = theme;

	switch (theme) {
		case Theme::Dark: {
			QApplication::setStyle(QStyleFactory::create("Fusion"));

			QPalette p;
			p.setColor(QPalette::Window, QColor(53, 53, 53));
			p.setColor(QPalette::WindowText, Qt::white);
			p.setColor(QPalette::Base, QColor(25, 25, 25));
			p.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
			p.setColor(QPalette::ToolTipBase, Qt::white);
			p.setColor(QPalette::ToolTipText, Qt::white);
			p.setColor(QPalette::Text, Qt::white);
			p.setColor(QPalette::Button, QColor(53, 53, 53));
			p.setColor(QPalette::ButtonText, Qt::white);
			p.setColor(QPalette::BrightText, Qt::red);
			p.setColor(QPalette::Link, QColor(42, 130, 218));

			p.setColor(QPalette::Highlight, QColor(42, 130, 218));
			p.setColor(QPalette::HighlightedText, Qt::black);
			qApp->setPalette(p);
			break;
		}

		case Theme::Light: {
			QApplication::setStyle(QStyleFactory::create("Fusion"));

			QPalette p;
			p.setColor(QPalette::Window, Qt::white);
			p.setColor(QPalette::WindowText, Qt::black);
			p.setColor(QPalette::Base, QColor(243, 243, 243));
			p.setColor(QPalette::AlternateBase, Qt::white);
			p.setColor(QPalette::ToolTipBase, Qt::black);
			p.setColor(QPalette::ToolTipText, Qt::black);
			p.setColor(QPalette::Text, Qt::black);
			p.setColor(QPalette::Button, Qt::white);
			p.setColor(QPalette::ButtonText, Qt::black);
			p.setColor(QPalette::BrightText, Qt::red);
			p.setColor(QPalette::Link, QColor(42, 130, 218));

			p.setColor(QPalette::Highlight, QColor(42, 130, 218));
			p.setColor(QPalette::HighlightedText, Qt::white);
			qApp->setPalette(p);
			break;
		}

		case Theme::System: {
			qApp->setPalette(this->style()->standardPalette());
			qApp->setStyle(QStyleFactory::create("WindowsVista"));
			qApp->setStyleSheet("");
			break;
		}
	}
}

void MainWindow::dumpRomFS() {
	auto folder = QFileDialog::getExistingDirectory(
		this, tr("Select folder to dump RomFS files to"), "", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
	);

	if (folder.isEmpty()) {
		return;
	}
	std::filesystem::path path(folder.toStdU16String());
	
	// TODO: This might break if the game accesses RomFS while we're dumping, we should move it to the emulator thread when we've got a message queue going
	messageQueueMutex.lock();
	RomFS::DumpingResult res = emu->dumpRomFS(path);
	messageQueueMutex.unlock();

	switch (res) {
		case RomFS::DumpingResult::Success: break; // Yay!
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

void MainWindow::dispatchMessage(const EmulatorMessage& message) {
	switch (message.type) {
		case MessageType::LoadROM:
			emu->loadROM(*message.path.p);
			// Clean up the allocated path
			delete message.path.p;
			break;

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