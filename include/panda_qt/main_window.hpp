#pragma once

#include <QApplication>
#include <QtWidgets>

#include "panda_qt/screen.hpp"

class MainWindow : public QMainWindow {
	Q_OBJECT

  private:
	ScreenWidget screen;

  public:
	MainWindow(QApplication* app, QWidget* parent = nullptr);
};