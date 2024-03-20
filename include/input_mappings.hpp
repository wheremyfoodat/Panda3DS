#pragma once

#include <unordered_map>

#include "helpers.hpp"
#include "services/hid.hpp"

struct InputMappings {
	using Scancode = u32;
	using Container = std::unordered_map<Scancode, u32>;

	u32 getMapping(Scancode scancode) const {
		auto it = container.find(scancode);
		return it != container.end() ? it->second : HID::Keys::Null;
	}

	void setMapping(Scancode scancode, u32 key) { container[scancode] = key; }
	static InputMappings defaultKeyboardMappings();

  private:
	Container container;
};
