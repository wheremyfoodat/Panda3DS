#include "input_mappings.hpp"

#include <SDL.h>

InputMappings InputMappings::defaultKeyboardMappings() {
	InputMappings mappings;
	mappings.setMapping(SDLK_l, HID::Keys::A);
	mappings.setMapping(SDLK_k, HID::Keys::B);
	mappings.setMapping(SDLK_o, HID::Keys::X);
	mappings.setMapping(SDLK_i, HID::Keys::Y);
	mappings.setMapping(SDLK_q, HID::Keys::L);
	mappings.setMapping(SDLK_p, HID::Keys::R);
	mappings.setMapping(SDLK_UP, HID::Keys::Up);
	mappings.setMapping(SDLK_DOWN, HID::Keys::Down);
	mappings.setMapping(SDLK_RIGHT, HID::Keys::Right);
	mappings.setMapping(SDLK_LEFT, HID::Keys::Left);
	mappings.setMapping(SDLK_RETURN, HID::Keys::Start);
	mappings.setMapping(SDLK_BACKSPACE, HID::Keys::Select);
	mappings.setMapping(SDLK_w, HID::Keys::CirclePadUp);
	mappings.setMapping(SDLK_s, HID::Keys::CirclePadDown);
	mappings.setMapping(SDLK_d, HID::Keys::CirclePadRight);
	mappings.setMapping(SDLK_a, HID::Keys::CirclePadLeft);

	return mappings;
}