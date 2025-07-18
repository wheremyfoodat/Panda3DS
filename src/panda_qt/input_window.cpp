#include "panda_qt/input_window.hpp"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "input_mappings.hpp"
#include "services/hid.hpp"

InputWindow::InputWindow(QWidget* parent) : QDialog(parent) {
	auto mainLayout = new QVBoxLayout(this);

	QStringList actions = {
		"A",
		"B",
		"X",
		"Y",
		"L",
		"R",
		"ZL",
		"ZR",
		"Start",
		"Select",
		"D-Pad Up",
		"D-Pad Down",
		"D-Pad Left",
		"D-Pad Right",
		"CirclePad Up",
		"CirclePad Down",
		"CirclePad Left",
		"CirclePad Right",
	};

	for (const QString& action : actions) {
		auto row = new QHBoxLayout();
		row->addWidget(new QLabel(action));

		auto button = new QPushButton(tr("Not set"));
		buttonMap[action] = button;
		keyMappings[action] = QKeySequence();

		connect(button, &QPushButton::clicked, this, [=, this]() { startKeyCapture(action); });

		row->addWidget(button);
		mainLayout->addLayout(row);
	}

	auto resetButton = new QPushButton(tr("Reset Defaults"));
	connect(resetButton, &QPushButton::pressed, this, [&]() {
		// Restore the keymappings to the default ones for Qt
		auto defaultMappings = InputMappings::defaultKeyboardMappings();
		loadFromMappings(defaultMappings);

		emit mappingsChanged();
	});

	mainLayout->addWidget(resetButton);
	installEventFilter(this);
}

void InputWindow::startKeyCapture(const QString& action) {
	waitingForAction = action;
	grabKeyboard();
}

bool InputWindow::eventFilter(QObject* obj, QEvent* event) {
	// If we're waiting for a button to be inputted, handle the keypress
	if (!waitingForAction.isEmpty() && event->type() == QEvent::KeyPress) {
		QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
		QKeySequence key(keyEvent->key());

		// If this key is already bound to something else, unbind it
		for (auto it = keyMappings.begin(); it != keyMappings.end(); ++it) {
			if (it.key() != waitingForAction && it.value() == key) {
				it.value() = QKeySequence();
				buttonMap[it.key()]->setText(tr("Not set"));
				break;
			}
		}

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
				QKeySequence qkey(scancode);
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
			InputMappings::Scancode scancode = qkey[0].key();
			u32 key = HID::Keys::nameToKey(action.toStdString());

			if (key != HID::Keys::Null) {
				mappings.setMapping(scancode, key);
			}
		}
	}
}