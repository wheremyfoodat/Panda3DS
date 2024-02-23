#pragma once

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QPalette>
#include <QWidget>
#include <QtWidgets>
#include <functional>
#include <utility>

#include "config.hpp"

class ConfigWindow : public QDialog {
	Q_OBJECT

  private:
	using ConfigCallback = std::function<void()>;

	enum class Theme : int {
		System = 0,
		Light = 1,
		Dark = 2,
		GreetingsCat = 3,
	};

	Theme currentTheme;
	QComboBox* themeSelect = nullptr;

	// The config class holds a copy of the emulator config which it edits and sends
	// over to the emulator
	EmulatorConfig config;
	ConfigCallback updateConfig;
	void setTheme(Theme theme);

  public:
	ConfigWindow(ConfigCallback callback, const EmulatorConfig& config, QWidget* parent = nullptr);
	~ConfigWindow();

	EmulatorConfig& getConfig() { return config; }
};