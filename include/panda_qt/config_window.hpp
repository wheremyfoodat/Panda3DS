#pragma once

#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QPalette>
#include <QWidget>
#include <QtWidgets>

class ConfigWindow : public QDialog {
	Q_OBJECT

  private:
	enum class Theme : int {
		System = 0,
		Light = 1,
		Dark = 2,
		GreetingsCat = 3,
		IceCreamAndJelly = 4,
	};

	Theme currentTheme;
	QComboBox* themeSelect = nullptr;

	void setTheme(Theme theme);

  public:
	ConfigWindow(QWidget* parent = nullptr);
	~ConfigWindow();
};
