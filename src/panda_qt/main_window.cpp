#include "panda_qt/main_window.hpp"

MainWindow::MainWindow(QApplication* app, QWidget* parent) : QMainWindow(parent), screen(this) {
	setWindowTitle("Alber");
	// Enable drop events for loading ROMs
	setAcceptDrops(true);
	resize(400, 240 * 2);
	screen.show();

	// Set our menu bar up
	menuBar = new QMenuBar(this);
	setMenuBar(menuBar);

	auto pandaMenu = menuBar->addMenu(tr("PANDA"));
	auto pandaAction = pandaMenu->addAction(tr("panda..."));

	// Set up theme selection
	setTheme(Theme::Dark);
	themeSelect = new QComboBox(this);
	themeSelect->addItem("System");
	themeSelect->addItem("Light");
	themeSelect->addItem("Dark");
	themeSelect->setCurrentIndex(static_cast<int>(currentTheme));

	themeSelect->setGeometry(40, 40, 100, 50);
	themeSelect->show();
	connect(themeSelect, &QComboBox::currentIndexChanged, this, [&](int index) { setTheme(static_cast<Theme>(index)); });

	emu = new Emulator();

	// The emulator graphics context for the thread should be initialized in the emulator thread due to how GL contexts work 
	emuThread = std::thread([&]() {
		const RendererType rendererType = emu->getConfig().rendererType;

		if (rendererType == RendererType::OpenGL || rendererType == RendererType::Software || rendererType == RendererType::Null) {
			// Make GL context current for this thread, enable VSync
			GL::Context* glContext = screen.getGLContext();
			glContext->MakeCurrent();
			glContext->SetSwapInterval(1);

			emu->initGraphicsContext(glContext);
		} else {
			Helpers::panic("Unsupported renderer type for the Qt backend! Vulkan on Qt is currently WIP, try the SDL frontend instead!");
		}

		bool success = emu->loadROM("OoT.3ds");
		if (!success) {
			Helpers::panic("Failed to load ROM");
		}

		while (true) {
			emu->runFrame();
			screen.getGLContext()->SwapBuffers();
		}
	});
}

// Cleanup when the main window closes
MainWindow::~MainWindow() {
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