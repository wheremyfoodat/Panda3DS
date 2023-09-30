#include "panda_qt/main_window.hpp"

MainWindow::MainWindow(QApplication* app, QWidget* parent) : QMainWindow(parent), screen(this) {
	setWindowTitle("Alber");
	// Enable drop events for loading ROMs
	setAcceptDrops(true);
	resize(320, 240);
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
}

MainWindow::~MainWindow() { delete menuBar; }

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