#pragma once

#include <QApplication>
#include <QComboBox>
#include <QMenuBar>
#include <QPalette>
#include <QtWidgets>
#include <atomic>
#include <thread>
#include <filesystem>

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

	std::atomic<bool> appRunning = true; // Is the application itself running?
	std::atomic<bool> needToLoadROM = false;
	std::filesystem::path romToLoad = "";

	ScreenWidget screen;
	QComboBox* themeSelect = nullptr;
	QMenuBar* menuBar = nullptr;

	Theme currentTheme;
	void setTheme(Theme theme);
	void swapEmuBuffer();
	void emuThreadMainLoop();
	void selectROM();
	void dumpRomFS();

	// Tracks whether we are using an OpenGL-backed renderer or a Vulkan-backed renderer
	bool usingGL = false;
	bool usingVk = false;

  public:
	MainWindow(QApplication* app, QWidget* parent = nullptr);
	~MainWindow();
};