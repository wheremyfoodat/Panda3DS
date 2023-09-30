#pragma once

#include <QApplication>
#include <QComboBox>
#include <QMenuBar>
#include <QPalette>
#include <QtWidgets>

#include "panda_qt/screen.hpp"

class MainWindow : public QMainWindow {
	Q_OBJECT

  private:
	enum class Theme : int {
		System = 0,
		Light = 1,
		Dark = 2,
	};

	QComboBox* themeSelect = nullptr;
	QMenuBar* menuBar = nullptr;
	ScreenWidget screen;

	Theme currentTheme;
	void setTheme(Theme theme);

  public:
	MainWindow(QApplication* app, QWidget* parent = nullptr);
	~MainWindow();
};