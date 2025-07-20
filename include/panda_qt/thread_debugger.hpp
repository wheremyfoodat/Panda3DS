#pragma once

#include <QString>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QtWidgets>

#include "emulator.hpp"

class ThreadDebugger : public QWidget {
	Q_OBJECT
	Emulator* emu;

	QVBoxLayout* mainLayout;
	QTableWidget* threadTable;

  public:
	ThreadDebugger(Emulator* emu, QWidget* parent = nullptr);
	void update();

  private:
	void setListItem(int row, int column, const QString& str);
	void setListItem(int row, int column, const std::string& str);
};