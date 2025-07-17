#include "panda_qt/input_window.hpp"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QVBoxLayout>

#include "services/hid.hpp"

InputWindow::InputWindow(QWidget* parent) : QDialog(parent) {
	auto mainLayout = new QVBoxLayout(this);

	QStringList actions = {
		"A", "B", "X", "Y", "L", "R", "ZL", "ZR", "Start", "Select", "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right",
	};

	for (const QString& action : actions) {
		auto row = new QHBoxLayout;
		row->addWidget(new QLabel(action));

		auto button = new QPushButton("Not set");
		buttonMap[action] = button;
		keyMappings[action] = QKeySequence();

		connect(button, &QPushButton::clicked, this, [=]() { startKeyCapture(action); });

		row->addWidget(button);
		mainLayout->addLayout(row);
	}

	installEventFilter(this);
}

void InputWindow::startKeyCapture(const QString& action) {
	waitingForAction = action;
	grabKeyboard();
}

bool InputWindow::eventFilter(QObject* obj, QEvent* event) {
	if (!waitingForAction.isEmpty() && event->type() == QEvent::KeyPress) {
		QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
		QKeySequence key(keyEvent->key());

		keyMappings[waitingForAction] = key;
		buttonMap[waitingForAction]->setText(key.toString());

		releaseKeyboard();
		waitingForAction.clear();

		emit mappingsChanged();
		return true;
	}

	return false;
}

void InputWindow::loadFromMappings(const InputMappings& mappings) {
	for (const auto& action : buttonMap.keys()) {
		u32 key = HID::Keys::nameToKey(action.toStdString());

		for (const auto& [scancode, mappedKey] : mappings) {
			if (mappedKey == key) {
				QKeySequence qkey(QString::fromStdString(InputMappings::scancodeToName(scancode)));
				keyMappings[action] = qkey;
				buttonMap[action]->setText(qkey.toString());
				break;
			}
		}
	}
}

void InputWindow::applyToMappings(InputMappings& mappings) const {
	// Clear existing keyboard mappings before mapping the buttons
	mappings = InputMappings();

	for (const auto& action : keyMappings.keys()) {
		const QKeySequence& qkey = keyMappings[action];

		if (!qkey.isEmpty()) {
			InputMappings::Scancode scancode = InputMappings::nameToScancode(qkey.toString().toStdString());
			u32 key = HID::Keys::nameToKey(action.toStdString());

			if (key != HID::Keys::Null) {
				mappings.setMapping(scancode, key);
			}
		}
	}
}