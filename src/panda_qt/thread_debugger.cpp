#include "panda_qt/thread_debugger.hpp"

#include <fmt/format.h>

static QString threadStatusToQString(ThreadStatus status) {
	switch (status) {
		case ThreadStatus::Running: return QObject::tr("Running");
		case ThreadStatus::Ready: return QObject::tr("Ready");
		case ThreadStatus::WaitArbiter: return QObject::tr("Waiting on arbiter");
		case ThreadStatus::WaitSleep: return QObject::tr("Sleeping");
		case ThreadStatus::WaitSync1: return QObject::tr("WaitSync (1)");
		case ThreadStatus::WaitSyncAny: return QObject::tr("WaitSync (Any)");
		case ThreadStatus::WaitSyncAll: return QObject::tr("WaitSync (All)");
		case ThreadStatus::WaitIPC: return QObject::tr("Waiting for IPC");
		case ThreadStatus::Dormant: return QObject::tr("Dormant");
		case ThreadStatus::Dead: return QObject::tr("Dead");
		default: return QObject::tr("Unknown thread status");
	}
}

ThreadDebugger::ThreadDebugger(Emulator* emu, QWidget* parent) : emu(emu), QWidget(parent, Qt::Window) {
	setWindowTitle(tr("Thread Debugger"));
	resize(700, 600);

	mainLayout = new QVBoxLayout(this);
	threadTable = new QTableWidget(this);
	threadTable->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

	mainLayout->addWidget(threadTable);
}

void ThreadDebugger::update() {
	threadTable->clear();

	threadTable->setSelectionMode(QAbstractItemView::NoSelection);
	threadTable->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	threadTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
	threadTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	threadTable->setColumnCount(5);
	threadTable->verticalHeader()->setVisible(false);
	threadTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	threadTable->setHorizontalHeaderLabels(QStringList({"ID", "Status", "PC", "Entrypoint", "Stack Top"}));

	const auto threads = emu->getKernel().getMainProcessThreads();
	const usize count = threads.size();
	threadTable->setRowCount(count);

	for (int i = 0; i < count; i++) {
		const auto& thread = threads[i];
		setListItem(i, 0, fmt::format("{}", thread.index));
		setListItem(i, 1, threadStatusToQString(thread.status));
		setListItem(i, 2, fmt::format("{:08X}", thread.gprs[15]));
		setListItem(i, 3, fmt::format("{:08X}", thread.entrypoint));
		setListItem(i, 4, fmt::format("{:08X}", thread.initialSP));
	}
}

void ThreadDebugger::setListItem(int row, int column, const std::string& str) { setListItem(row, column, QString::fromStdString(str)); }

void ThreadDebugger::setListItem(int row, int column, const QString& str) {
	QTableWidgetItem* item = new QTableWidgetItem();
	QWidget* widget = new QWidget(this);
	QVBoxLayout* layout = new QVBoxLayout(widget);
	QLabel* label = new QLabel(widget);

	layout->setAlignment(Qt::AlignVCenter);
	layout->setContentsMargins(5, 0, 5, 0);

	label->setText(str);
	layout->addWidget(label);
	widget->setLayout(layout);

	threadTable->setItem(row, column, item);
	threadTable->setCellWidget(row, column, widget);
}
