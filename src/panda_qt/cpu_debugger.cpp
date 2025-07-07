#include "panda_qt/cpu_debugger.hpp"

#include <fmt/format.h>

#include <QCheckBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <limits>
#include <span>
#include <utility>

#include "capstone.hpp"

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

CPUDebugger::CPUDebugger(Emulator* emulator, QWidget* parent) : emu(emulator), QWidget(parent, Qt::Window) {
	setWindowTitle(tr("CPU debugger"));
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
	addressInput->setMaximumWidth(100);

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

	// Setup overlay for when the widget is disabled
	disabledOverlay = new DisabledWidgetOverlay(this, tr("Pause the emulator to use the CPU Debugger"));
	disabledOverlay->resize(size());  // Fill the whole screen
	disabledOverlay->raise();
	disabledOverlay->hide();

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
	connect(verticalScrollBar, &QScrollBar::valueChanged, this, &CPUDebugger::updateDisasm);
	registerTextEdit->setReadOnly(true);

	connect(goToPCButton, &QPushButton::clicked, this, [&]() { scrollToPC(); });

	// We have a QTimer that triggers every 500ms to update our widget when it's active
	updateTimer = new QTimer(this);
	connect(updateTimer, &QTimer::timeout, this, &CPUDebugger::update);

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

void CPUDebugger::enable() {
	enabled = true;

	disabledOverlay->hide();
	scrollToPC();

	// Update the widget every 500ms
	updateTimer->start(500);
	update();
}

void CPUDebugger::disable() {
	enabled = false;

	updateTimer->stop();
	disabledOverlay->show();
}

void CPUDebugger::update() {
	if (enabled) {
		if (followPC) {
			scrollToPC();
		}

		updateDisasm();
		updateRegisters();
	}
}

void CPUDebugger::updateDisasm() {
	int currentRow = disasmListWidget->currentRow();
	disasmListWidget->clear();

	auto [firstLine, lineCount] = getVisibleLineRange(disasmListWidget, verticalScrollBar);
	const u32 startPC = (firstLine + 3) & ~3;  // Align PC to 4 bytes
	const u32 endPC = startPC + lineCount * sizeof(u32);

	auto& cpu = emu->getCPU();
	auto& mem = emu->getMemory();
	u32 pc = cpu.getReg(15);

	Common::CapstoneDisassembler disassembler(CS_ARCH_ARM, CS_MODE_ARM);
	std::string disassembly;

	for (u32 addr = startPC; addr < endPC; addr += sizeof(u32)) {
		if (auto pointer = (u32*)mem.getReadPointer(addr)) {
			const u32 instruction = *pointer;

			// Convert instruction to byte array to pass to Capstone
			const std::array<u8, 4> bytes = {
				u8(instruction & 0xff),
				u8((instruction >> 8) & 0xff),
				u8((instruction >> 16) & 0xff),
				u8((instruction >> 24) & 0xff),
			};

			disassembler.disassemble(disassembly, pc, std::span(bytes));
			disassembly = fmt::format("{:08X}     |     {}", addr, disassembly);

			QListWidgetItem* item = new QListWidgetItem(QString::fromStdString(disassembly));
			if (addr == pc) {
				item->setBackground(Qt::darkGreen);
			}
			disasmListWidget->addItem(item);
		} else
			disasmListWidget->addItem(QString::fromStdString(fmt::format("{:08X}     |     ???", addr)));
	}

	disasmListWidget->setCurrentRow(currentRow);
}

void CPUDebugger::scrollToPC() {
	u32 pc = emu->getCPU().getReg(15);
	verticalScrollBar->setValue(pc);
}

void CPUDebugger::updateRegisters() {
	auto& cpu = emu->getCPU();
	const std::span<u32, 16> gprs = cpu.regs();
	const std::span<u32, 32> fprs = cpu.fprs();
	const u32 pc = gprs[15];
	const u32 cpsr = cpu.getCPSR();
	const u32 fpscr = cpu.getFPSCR();

	std::string text = "";
	text.reserve(2048);

	text += fmt::format("PC: {:08X}\nCPSR: {:08X}\nFPSCR: {:08X}\n", pc, cpsr, fpscr);

	text += "\nGeneral Purpose Registers\n";
	for (int i = 0; i < 10; i++) {
		text += fmt::format("r{:01d}:     0x{:08X}\n", i, gprs[i]);
	}
	for (int i = 10; i < 16; i++) {
		text += fmt::format("r{:02d}:    0x{:08X}\n", i, gprs[i]);
	}

	text += "\nFloating Point Registers\n";
	for (int i = 0; i < 10; i++) {
		text += fmt::format("f{:01d}:     {:f}\n", i, Helpers::bit_cast<float, u32>(fprs[i]));
	}
	for (int i = 10; i < 32; i++) {
		text += fmt::format("f{:01d}:    {:f}\n", i, Helpers::bit_cast<float, u32>(fprs[i]));
	}

	registerTextEdit->setPlainText(QString::fromStdString(text));
}

bool CPUDebugger::eventFilter(QObject* obj, QEvent* event) {
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

void CPUDebugger::showEvent(QShowEvent* event) {
	QWidget::showEvent(event);

	enable();
}

// Scroll 1 instruction up or down when the arrow keys are pressed and we're at the edge of the disassembly list
void CPUDebugger::keyPressEvent(QKeyEvent* event) {
	constexpr usize instructionSize = sizeof(u32);

	if (event->key() == Qt::Key_Up) {
		verticalScrollBar->setValue(verticalScrollBar->value() - instructionSize);
	} else if (event->key() == Qt::Key_Down) {
		verticalScrollBar->setValue(verticalScrollBar->value() + instructionSize);
	} else {
		QWidget::keyPressEvent(event);
	}
}

void CPUDebugger::resizeEvent(QResizeEvent* event) {
	QWidget::resizeEvent(event);
	disabledOverlay->resize(event->size());
	verticalScrollBar->setPageStep(getLinesInViewport(disasmListWidget));

	update();
}