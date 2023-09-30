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

	auto themeButton = new QPushButton(tr("Dark theme"), this);
	themeButton->setGeometry(40, 40, 100, 400);
	themeButton->show();
	connect(themeButton, &QPushButton::pressed, this, [&]() { setDarkTheme(); });
}

MainWindow::~MainWindow() { delete menuBar; }

void MainWindow::setDarkTheme() {
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
}