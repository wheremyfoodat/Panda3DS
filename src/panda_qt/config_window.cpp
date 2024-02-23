#include "panda_qt/config_window.hpp"

#include <QHBoxLayout>
#include <QSizePolicy>
#include <QVBoxLayout>

ConfigWindow::ConfigWindow(ConfigCallback callback, const EmulatorConfig& emuConfig, QWidget* parent) : QDialog(parent), config(emuConfig) {
	setWindowTitle(tr("Configuration"));
	updateConfig = std::move(callback);

	// Initialize the widget list and the widget container widgets
	widgetList = new QListWidget(this);
	widgetContainer = new QStackedWidget(this);

	helpText = new QTextEdit(this);
	helpText->setReadOnly(true);

	helpText->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	helpText->setFixedHeight(50);

	widgetList->setMinimumWidth(100);
	widgetList->setMaximumWidth(100);
	widgetList->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

	widgetList->setCurrentRow(0);
	widgetContainer->setCurrentIndex(0);

	connect(widgetList, &QListWidget::currentRowChanged, this, [&](int row) {
		widgetContainer->setCurrentIndex(row);
		helpText->setText(helpTexts[row]);
	});

	QVBoxLayout* mainLayout = new QVBoxLayout;
	QHBoxLayout* hLayout = new QHBoxLayout;

	// Set up widget layouts
	setLayout(mainLayout);
	mainLayout->addLayout(hLayout);
	mainLayout->addWidget(helpText);

	hLayout->setAlignment(Qt::AlignLeft);
	hLayout->addWidget(widgetList);
	hLayout->addWidget(widgetContainer);

	// Set up theme selection
	setTheme(Theme::Dark);
	themeSelect = new QComboBox(widgetContainer);
	themeSelect->addItem(tr("System"));
	themeSelect->addItem(tr("Light"));
	themeSelect->addItem(tr("Dark"));
	themeSelect->addItem(tr("Greetings Cat"));
	themeSelect->setCurrentIndex(static_cast<int>(currentTheme));

	themeSelect->setGeometry(40, 40, 100, 50);
	themeSelect->show();
	connect(themeSelect, &QComboBox::currentIndexChanged, this, [&](int index) { setTheme(static_cast<Theme>(index)); });

	QCheckBox* useShaderJIT = new QCheckBox(tr("Enable shader recompiler"), widgetContainer);
	useShaderJIT->setChecked(config.shaderJitEnabled);
	useShaderJIT->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

	// Add all our settings widgets to our widget list
	addWidget(themeSelect, tr("General"), ":/docs/img/settings_icon.png", tr("General emulator settings"));
	addWidget(useShaderJIT, tr("UI"), ":/docs/img/sparkling_icon.png", tr("User Interface (UI) settings"));
	addWidget(useShaderJIT, tr("Graphics"), ":/docs/img/display_icon.png", tr("Graphics emulation and output settings"));
	addWidget(useShaderJIT, tr("Audio"), ":/docs/img/speaker_icon.png", tr("Audio emulation and output settings"));
	
	helpText->setText(helpTexts[0]);

	connect(useShaderJIT, &QCheckBox::toggled, this, [this, useShaderJIT]() {
		config.shaderJitEnabled = useShaderJIT->isChecked();
		updateConfig();
	});
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

		case Theme::System: {
			qApp->setPalette(this->style()->standardPalette());
			qApp->setStyle(QStyleFactory::create("WindowsVista"));
			qApp->setStyleSheet("");
			break;
		}
	}
}

void ConfigWindow::addWidget(QWidget* widget, QString title, QString icon, QString helpText) {
	const int index = widgetList->count();

	QListWidgetItem* item = new QListWidgetItem(widgetList);
	item->setText(title);
	if (!icon.isEmpty()) {
		item->setIcon(QIcon::fromTheme(icon));
	}

	widgetContainer->addWidget(widget);

	if (index >= settingWidgetCount) {
		Helpers::panic("Qt: ConfigWindow::settingWidgetCount has not been updated correctly!");
	}
	helpTexts[index] = std::move(helpText);
}

ConfigWindow::~ConfigWindow() { delete themeSelect; }