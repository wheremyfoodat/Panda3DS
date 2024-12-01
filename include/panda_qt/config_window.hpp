#pragma once

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QListWidget>
#include <QPalette>
#include <QStackedWidget>
#include <QTextEdit>
#include <QWidget>
#include <QtWidgets>
#include <array>
#include <functional>
#include <utility>

#include "emulator.hpp"

class ConfigWindow : public QDialog {
	Q_OBJECT

  private:
	using ConfigCallback = std::function<void()>;

	enum class Theme : int {
		System = 0,
		Light = 1,
		Dark = 2,
		GreetingsCat = 3,
		Cream = 4,
	};

	Theme currentTheme;
	QTextEdit* helpText = nullptr;
	QListWidget* widgetList = nullptr;
	QStackedWidget* widgetContainer = nullptr;

	static constexpr size_t settingWidgetCount = 6;
	std::array<QString, settingWidgetCount> helpTexts;

	// The config class holds a copy of the emulator config which it edits and sends
	// over to the emulator in a thread-safe manner
	EmulatorConfig config;
	ConfigCallback updateConfig;

	void addWidget(QWidget* widget, QString title, QString icon, QString helpText);
	void setTheme(Theme theme);

  public:
	ConfigWindow(ConfigCallback callback, const EmulatorConfig& config, QWidget* parent = nullptr);
	~ConfigWindow();

	EmulatorConfig& getConfig() { return config; }

  private:
	Emulator* emu;
};
