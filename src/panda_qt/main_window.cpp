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

	auto fileMenu = menuBar->addMenu(tr("File"));
	auto pandaAction = fileMenu->addAction(tr("panda..."));
	connect(pandaAction, &QAction::triggered, this, &MainWindow::selectROM);

	auto emulationMenu = menuBar->addMenu(tr("Emulation"));
	auto helpMenu = menuBar->addMenu(tr("Help"));
	auto aboutMenu = menuBar->addMenu(tr("About"));

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
		if (needToLoadROM.load()) {
			bool success = emu->loadROM(romToLoad);
			if (!success) {
				printf("Failed to load ROM");
			}

			needToLoadROM.store(false, std::memory_order::seq_cst);
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
	// Are we already waiting for a ROM to be loaded? Then complain about it!
	if (needToLoadROM.load()) {
		QMessageBox::warning(this, tr("Already loading ROM"), tr("Panda3DS is already busy loading a ROM, please wait"));
		return;
	}

	auto path =
		QFileDialog::getOpenFileName(this, tr("Select 3DS ROM to load"), "", tr("Nintendo 3DS ROMs (*.3ds *.cci *.cxi *.app *.3dsx *.elf *.axf)"));

	if (!path.isEmpty()) {
		romToLoad = path.toStdU16String();
		needToLoadROM.store(true, std::memory_order_seq_cst);
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