#include "panda_qt/dsp_debugger.hpp"

#include <fmt/format.h>

#include <QCheckBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <limits>
#include <utility>

#include "audio/dsp_core.hpp"
#include "teakra/disassembler.h"

#undef Assert
#undef UNREACHABLE
#include "teakra/impl/register.h"

// TODO: Make this actually thread-safe by having it only work when paused
static int getLinesInViewport(QListWidget* listWidget) {
	auto viewportHeight = listWidget->viewport()->height();
	QFontMetrics fm = QFontMetrics(listWidget->font());
	auto lineHeight = fm.height();

	return int(viewportHeight / lineHeight);
}

static std::pair<int, int> getVisibleLineRange(QListWidget* listWidget, QScrollBar* scrollBar) {
	int firstLine = scrollBar->value();
	int lineCount = getLinesInViewport(listWidget);

	return {firstLine, lineCount};
}

DSPDebugger::DSPDebugger(Emulator* emulator, QWidget* parent) : emu(emulator), QWidget(parent, Qt::Window) {
	setWindowTitle(tr("DSP debugger"));
	resize(1000, 600);

	QGridLayout* gridLayout = new QGridLayout(this);
	QHBoxLayout* horizontalLayout = new QHBoxLayout();

	// Set up the top line widgets
	QPushButton* goToAddressButton = new QPushButton(tr("Go to address"), this);
	QPushButton* goToPCButton = new QPushButton(tr("Go to PC"), this);
	QCheckBox* followPCCheckBox = new QCheckBox(tr("Follow PC"), this);
	addressInput = new QLineEdit(this);

	horizontalLayout->addWidget(goToAddressButton);
	horizontalLayout->addWidget(goToPCButton);
	horizontalLayout->addWidget(followPCCheckBox);
	horizontalLayout->addWidget(addressInput);

	followPCCheckBox->setChecked(followPC);
	connect(followPCCheckBox, &QCheckBox::toggled, this, [&](bool checked) { followPC = checked; });

	addressInput->setPlaceholderText(tr("Address to jump to"));
	addressInput->setMaximumWidth(150);

	gridLayout->addLayout(horizontalLayout, 0, 0);

	// Disassembly list on the left, scrollbar in the middle, register view on the right
	disasmListWidget = new QListWidget(this);
	gridLayout->addWidget(disasmListWidget, 1, 0);

	verticalScrollBar = new QScrollBar(Qt::Vertical, this);
	gridLayout->addWidget(verticalScrollBar, 1, 1);

	registerTextEdit = new QPlainTextEdit(this);
	registerTextEdit->setEnabled(true);
	registerTextEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	registerTextEdit->setMaximumWidth(800);
	gridLayout->addWidget(registerTextEdit, 1, 2);

	// Setup overlay for when the debugger is disabled
	disabledOverlay = new DisabledWidgetOverlay(this, tr("Pause the emulator to use the DSP Debugger"));
	disabledOverlay->resize(size());  // Fill the whole widget
	disabledOverlay->raise();
	disabledOverlay->hide();

	// Setup overlay for when the register widget is disabled
	disabledRegisterEditOverlay = new DisabledWidgetOverlay(registerTextEdit, tr("Register view is only supported\nwith LLE DSP"));
	disabledRegisterEditOverlay->resize(registerTextEdit->size());
	disabledRegisterEditOverlay->raise();
	disabledRegisterEditOverlay->hide();

	// Use a monospace font for the disassembly to align it
	QFont mono_font = QFont("Courier New");
	mono_font.setStyleHint(QFont::Monospace);
	disasmListWidget->setFont(mono_font);
	registerTextEdit->setFont(mono_font);

	// Forward scrolling from the list widget to our scrollbar
	disasmListWidget->installEventFilter(this);

	// Annoyingly, due to a Qt limitation we can't set it to U32_MAX
	verticalScrollBar->setRange(0, std::numeric_limits<s32>::max());
	verticalScrollBar->setSingleStep(8);
	verticalScrollBar->setPageStep(getLinesInViewport(disasmListWidget));
	verticalScrollBar->show();
	connect(verticalScrollBar, &QScrollBar::valueChanged, this, &DSPDebugger::updateDisasm);
	registerTextEdit->setReadOnly(true);

	connect(goToPCButton, &QPushButton::clicked, this, [&]() { scrollToPC(); });

	// We have a QTimer that triggers every 500ms to update our widget when it's active
	updateTimer = new QTimer(this);
	connect(updateTimer, &QTimer::timeout, this, &DSPDebugger::update);

	// Go to address when the "Go to address" button is pressed, or when we press enter inside the address input box
	connect(goToAddressButton, &QPushButton::clicked, this, [&]() {
		QString text = addressInput->text().trimmed();

		bool validAddr = false;
		u32 addr = text.toUInt(&validAddr, 16);  // Parse address as hex
		if (validAddr) {
			verticalScrollBar->setValue(addr);
		} else {
			addressInput->setText(tr("Invalid hexadecimal address"));
		}
	});
	connect(addressInput, &QLineEdit::returnPressed, goToAddressButton, &QPushButton::click);

	disable();
	hide();
}

void DSPDebugger::enable() {
	enabled = true;

	disabledOverlay->hide();
	scrollToPC();

	// Update the widget every 500ms
	updateTimer->start(500);
	update();
}

void DSPDebugger::disable() {
	enabled = false;

	updateTimer->stop();
	disabledOverlay->show();
}

void DSPDebugger::update() {
	if (enabled) {
		if (followPC) {
			scrollToPC();
		}

		updateDisasm();
		updateRegisters();
	}
}

void DSPDebugger::updateDisasm() {
	int currentRow = disasmListWidget->currentRow();
	disasmListWidget->clear();

	auto [firstLine, lineCount] = getVisibleLineRange(disasmListWidget, verticalScrollBar);
	const u32 startPC = firstLine;

	auto dsp = emu->getDSP();
	auto dspRam = dsp->getDspMemory();

	auto& mem = emu->getMemory();
	u32 pc = getPC();

	std::string disassembly;

	for (u32 addr = startPC, instructionCount = 0; instructionCount < lineCount; instructionCount++) {
		const u16 instruction = dsp->readProgramWord(addr);
		const bool needExpansion = Teakra::Disassembler::NeedExpansion(instruction);

		const u16 expansion = needExpansion ? dsp->readProgramWord(addr + 2) : u16(0);

		std::string disassembly = Teakra::Disassembler::Do(instruction, expansion);
		disassembly = fmt::format("{:08X}     |     {}", addr, disassembly);

		QListWidgetItem* item = new QListWidgetItem(QString::fromStdString(disassembly));
		if (addr == pc) {
			item->setBackground(Qt::darkGreen);
		}

		disasmListWidget->addItem(item);
		addr += needExpansion ? sizeof(u32) : sizeof(u16);
	}

	disasmListWidget->setCurrentRow(currentRow);
}

// This is only supported on the Teakra core, as other cores don't actually have a register contexts
u32 DSPDebugger::getPC() {
	auto dsp = emu->getDSP();
	auto dspType = dsp->getType();

	if (dspType == Audio::DSPCore::Type::Teakra) {
		auto regs = (Teakra::RegisterState*)dsp->getRegisters();
		return regs->pc | (u32(regs->prpage) << 18);
	} else {
		return 0;
	}
}

void DSPDebugger::scrollToPC() {
	u32 pc = getPC();
	verticalScrollBar->setValue(pc);
}

void DSPDebugger::updateRegisters() {
	auto dsp = emu->getDSP();
	auto dspType = dsp->getType();

	if (dspType == Audio::DSPCore::Type::Teakra) {
		std::string text = "";
		text.reserve(4096);

		auto regs = (Teakra::RegisterState*)dsp->getRegisters();
		text += fmt::format(
			"PC:            0x{:05X}\nProgram Page:  0x{:01X}\nStack Pointer: 0x{:04X}\n", regs->pc & 0x3FFFF, regs->prpage & 0xF, regs->sp
		);

		text += "\nGeneral Purpose Registers\n";
		for (int i = 0; i < 8; i++) {
			text += fmt::format("r{:01d}:     0x{:08X}\n", i, regs->r[i]);
		}

		text += "\nAccumulators (40-bit)\n";
		text += fmt::format("a0:     0x{:010X}\n", regs->a[0] & 0xFFFFFFFFFFull);
		text += fmt::format("a1:     0x{:010X}\n", regs->a[1] & 0xFFFFFFFFFFull);
		text += fmt::format("b0:     0x{:010X}\n", regs->b[0] & 0xFFFFFFFFFFull);
		text += fmt::format("b1:     0x{:010X}\n", regs->b[1] & 0xFFFFFFFFFFull);
		text += fmt::format("a1s:    0x{:010X}\n", regs->a1s & 0xFFFFFFFFFFull);
		text += fmt::format("b1s:    0x{:010X}\n", regs->b1s & 0xFFFFFFFFFFull);

		text += "\nMultiplication Unit\n";
		text += fmt::format("x0:     0x{:04X}\n", regs->x[0]);
		text += fmt::format("x1:     0x{:04X}\n", regs->x[1]);
		text += fmt::format("y0:     0x{:04X}\n", regs->y[0]);
		text += fmt::format("y1:     0x{:04X}\n", regs->y[1]);
		text += fmt::format("p0:     0x{:08X}\n", regs->p[0]);
		text += fmt::format("p1:     0x{:08X}\n", regs->p[1]);

		text += "\nOther Registers\n";
		text += fmt::format("mixp:   0x{:04X}\n", regs->mixp);
		text += fmt::format("sv:     0x{:04X}\n", regs->sv);
		text += fmt::format("Shift mode: {}\n", regs->s ? "Logic" : "Arithmetic");

		registerTextEdit->setPlainText(QString::fromStdString(text));
		disabledRegisterEditOverlay->hide();
	} else {
		registerTextEdit->setPlainText(QString::fromStdString(""));
		disabledRegisterEditOverlay->show();
	}
}

bool DSPDebugger::eventFilter(QObject* obj, QEvent* event) {
	// Forward scroll events from the list widget to the scrollbar
	if (obj == disasmListWidget && event->type() == QEvent::Wheel) {
		QWheelEvent* wheelEvent = (QWheelEvent*)event;

		int wheelSteps = wheelEvent->angleDelta().y() / 60;
		int newScrollValue = verticalScrollBar->value() - wheelSteps;
		newScrollValue = qBound(verticalScrollBar->minimum(), newScrollValue, verticalScrollBar->maximum());
		verticalScrollBar->setValue(newScrollValue);

		return true;
	}

	return QWidget::eventFilter(obj, event);
}

void DSPDebugger::showEvent(QShowEvent* event) {
	QWidget::showEvent(event);

	enable();
}

// Scroll 1 instruction up or down when the arrow keys are pressed and we're at the edge of the disassembly list
void DSPDebugger::keyPressEvent(QKeyEvent* event) {
	constexpr usize instructionSize = sizeof(u16);

	if (event->key() == Qt::Key_Up) {
		verticalScrollBar->setValue(verticalScrollBar->value() - instructionSize);
	} else if (event->key() == Qt::Key_Down) {
		verticalScrollBar->setValue(verticalScrollBar->value() + instructionSize);
	} else {
		QWidget::keyPressEvent(event);
	}
}

void DSPDebugger::resizeEvent(QResizeEvent* event) {
	QWidget::resizeEvent(event);
	disabledOverlay->resize(event->size());
	disabledRegisterEditOverlay->resize(registerTextEdit->size());

	verticalScrollBar->setPageStep(getLinesInViewport(disasmListWidget));
	update();
}