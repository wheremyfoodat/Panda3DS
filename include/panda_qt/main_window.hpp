#pragma once

#include <QApplication>
#include <QMenuBar>
#include <QPalette>
#include <QPushBUtton>
#include <QtWidgets>

#include "panda_qt/screen.hpp"

class MainWindow : public QMainWindow {
	Q_OBJECT

  private:
	QMenuBar* menuBar = nullptr;
	QPushButton* themeButton = nullptr;
	ScreenWidget screen;

	void setDarkTheme();

  public:
	MainWindow(QApplication* app, QWidget* parent = nullptr);
	~MainWindow();
};