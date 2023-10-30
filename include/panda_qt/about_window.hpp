#pragma once

#include <QApplication>
#include <QDialog>
#include <QWidget>

class AboutWindow : public QDialog {
	Q_OBJECT

  public:
	AboutWindow(QWidget* parent = nullptr);
};