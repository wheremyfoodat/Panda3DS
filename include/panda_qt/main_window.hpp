#pragma once

#include <QApplication>
#include <QComboBox>
#include <QMenuBar>
#include <QPalette>
#include <QtWidgets>
#include <thread>

#include "emulator.hpp"
#include "panda_qt/screen.hpp"

class MainWindow : public QMainWindow {
	Q_OBJECT

  private:
	enum class Theme : int {
		System = 0,
		Light = 1,
		Dark = 2,
	};

	// This would normally be an std::unique_ptr but it's shared between threads so definitely not
	Emulator* emu = nullptr;
	std::thread emuThread;

	QComboBox* themeSelect = nullptr;
	QMenuBar* menuBar = nullptr;
	ScreenWidget screen;

	Theme currentTheme;
	void setTheme(Theme theme);
	void swapEmuBuffer();

	// Tracks whether we are using an OpenGL-backed renderer or a Vulkan-backed renderer
	bool usingGL = false;
	bool usingVk = false;

  public:
	MainWindow(QApplication* app, QWidget* parent = nullptr);
	~MainWindow();
};