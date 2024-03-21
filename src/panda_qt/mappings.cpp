#include "input_mappings.hpp"

#include <QKeyEvent>

InputMappings InputMappings::defaultKeyboardMappings() {
	InputMappings mappings;
	mappings.setMapping(Qt::Key_L, HID::Keys::A);
	mappings.setMapping(Qt::Key_K, HID::Keys::B);
	mappings.setMapping(Qt::Key_O, HID::Keys::X);
	mappings.setMapping(Qt::Key_I, HID::Keys::Y);
	mappings.setMapping(Qt::Key_Q, HID::Keys::L);
	mappings.setMapping(Qt::Key_P, HID::Keys::R);
	mappings.setMapping(Qt::Key_Up, HID::Keys::Up);
	mappings.setMapping(Qt::Key_Down, HID::Keys::Down);
	mappings.setMapping(Qt::Key_Right, HID::Keys::Right);
	mappings.setMapping(Qt::Key_Left, HID::Keys::Left);
	mappings.setMapping(Qt::Key_Return, HID::Keys::Start);
	mappings.setMapping(Qt::Key_Backspace, HID::Keys::Select);
	mappings.setMapping(Qt::Key_W, HID::Keys::CirclePadUp);
	mappings.setMapping(Qt::Key_S, HID::Keys::CirclePadDown);
	mappings.setMapping(Qt::Key_D, HID::Keys::CirclePadRight);
	mappings.setMapping(Qt::Key_A, HID::Keys::CirclePadLeft);

	return mappings;
}