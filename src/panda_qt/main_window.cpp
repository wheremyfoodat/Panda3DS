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