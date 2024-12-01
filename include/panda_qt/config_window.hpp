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
	using IconCallback = std::function<void(const QString&)>;

	enum class Theme : int {
		System = 0,
		Light = 1,
		Dark = 2,
		GreetingsCat = 3,
		Cream = 4,
	};

	enum class WindowIcon : int {
		Rpog = 0,
		Rsyn = 1,
	};

	Theme currentTheme;
	WindowIcon currentIcon;

	QTextEdit* helpText = nullptr;
	QListWidget* widgetList = nullptr;
	QStackedWidget* widgetContainer = nullptr;

	static constexpr size_t settingWidgetCount = 6;
	std::array<QString, settingWidgetCount> helpTexts;

	// The config class holds a copy of the emulator config which it edits and sends
	// over to the emulator in a thread-safe manner
	EmulatorConfig config;

	ConfigCallback updateConfig;
	IconCallback updateIcon;

	void addWidget(QWidget* widget, QString title, QString icon, QString helpText);
	void setTheme(Theme theme);
	void setIcon(WindowIcon icon);

  public:
	ConfigWindow(ConfigCallback configCallback, IconCallback iconCallback, const EmulatorConfig& config, QWidget* parent = nullptr);
	~ConfigWindow();

	EmulatorConfig& getConfig() { return config; }

  private:
	Emulator* emu;
};
