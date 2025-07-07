#pragma once
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QWidget>

#include "emulator.hpp"
#include "panda_qt/disabled_widget_overlay.hpp"

class CPUDebugger : public QWidget {
	Q_OBJECT
	Emulator* emu;

	QListWidget* disasmListWidget;
	QScrollBar* verticalScrollBar;
	QPlainTextEdit* registerTextEdit;
	QTimer* updateTimer;
	QLineEdit* addressInput;

	DisabledWidgetOverlay* disabledOverlay;

	bool enabled = false;
	bool followPC = false;

  public:
	CPUDebugger(Emulator* emulator, QWidget* parent = nullptr);
	void enable();
	void disable();

  private:
	// Update the state of the disassembler. Qt events should always call update, not updateDisasm/updateRegister
	// As update properly handles thread safety
	void update();
	void updateDisasm();
	void updateRegisters();
	void scrollToPC();

	bool eventFilter(QObject* obj, QEvent* event) override;
	void showEvent(QShowEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;
	void keyPressEvent(QKeyEvent* event);
};
