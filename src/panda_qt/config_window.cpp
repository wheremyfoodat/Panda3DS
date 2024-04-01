#include "panda_qt/config_window.hpp"

ConfigWindow::ConfigWindow(QWidget* parent) : QDialog(parent) {
	setWindowTitle(tr("Configuration"));

	// Set up theme selection
	setTheme(Theme::Dark);
	themeSelect = new QComboBox(this);
	themeSelect->addItem(tr("System"));
	themeSelect->addItem(tr("Light"));
	themeSelect->addItem(tr("Dark"));
	themeSelect->addItem(tr("Greetings Cat"));
	themeSelect->addItem(tr("Cream"));
	themeSelect->setCurrentIndex(static_cast<int>(currentTheme));

	themeSelect->setGeometry(40, 40, 100, 50);
	themeSelect->show();
	connect(themeSelect, &QComboBox::currentIndexChanged, this, [&](int index) { setTheme(static_cast<Theme>(index)); });
}

void ConfigWindow::setTheme(Theme theme) {
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

		case Theme::GreetingsCat: {
			QApplication::setStyle(QStyleFactory::create("Fusion"));

			QPalette p;
			p.setColor(QPalette::Window, QColor(250, 207, 228));
			p.setColor(QPalette::WindowText, QColor(225, 22, 137));
			p.setColor(QPalette::Base, QColor(250, 207, 228));
			p.setColor(QPalette::AlternateBase, QColor(250, 207, 228));
			p.setColor(QPalette::ToolTipBase, QColor(225, 22, 137));
			p.setColor(QPalette::ToolTipText, QColor(225, 22, 137));
			p.setColor(QPalette::Text, QColor(225, 22, 137));
			p.setColor(QPalette::Button, QColor(250, 207, 228));
			p.setColor(QPalette::ButtonText, QColor(225, 22, 137));
			p.setColor(QPalette::BrightText, Qt::black);
			p.setColor(QPalette::Link, QColor(42, 130, 218));

			p.setColor(QPalette::Highlight, QColor(42, 130, 218));
			p.setColor(QPalette::HighlightedText, Qt::black);
			qApp->setPalette(p);
			break;
		}

		case Theme::Cream: {
			QApplication::setStyle(QStyleFactory::create("Fusion"));

			QPalette p;
			p.setColor(QPalette::Window, QColor(255, 229, 180));
			p.setColor(QPalette::WindowText, QColor(33, 37, 41));
			p.setColor(QPalette::Base, QColor(255, 229, 180));
			p.setColor(QPalette::AlternateBase, QColor(255, 229, 180));
			p.setColor(QPalette::ToolTipBase, QColor(33, 37, 41));
			p.setColor(QPalette::ToolTipText, QColor(33, 37, 41));
			p.setColor(QPalette::Text, QColor(33, 37, 41));
			p.setColor(QPalette::Button, QColor(255, 229, 180));
			p.setColor(QPalette::ButtonText, QColor(33, 37, 41));
			p.setColor(QPalette::BrightText, QColor(217, 113, 103));
			p.setColor(QPalette::Link, QColor(248, 148, 150));

			p.setColor(QPalette::Highlight, QColor(217, 113, 103));
			p.setColor(QPalette::HighlightedText, QColor(63, 33, 29));
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

ConfigWindow::~ConfigWindow() { delete themeSelect; }
