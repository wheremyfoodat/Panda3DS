#include "cheats.hpp"

Cheats::Cheats(Memory& mem, HIDService& hid) : ar(mem, hid) { reset(); }

void Cheats::reset() {
	clear();     // Clear loaded cheats
	ar.reset();  // Reset ActionReplay
}

u32 Cheats::addCheat(const Cheat& cheat) {
	cheatsLoaded = true;

	// Find an empty slot if a cheat was previously removed
	for (size_t i = 0; i < cheats.size(); i++) {
		if (cheats[i].type == CheatType::None) {
			cheats[i] = cheat;
			return i;
		}
	}

	// Otherwise, just add a new slot
	cheats.push_back(cheat);
	return cheats.size() - 1;
}

void Cheats::removeCheat(u32 id) {
	if (id >= cheats.size()) {
		return;
	}

	// Not using std::erase because we don't want to invalidate cheat IDs
	cheats[id].type = CheatType::None;
	cheats[id].instructions.clear();

	// Check if no cheats are loaded
	for (const auto& cheat : cheats) {
		if (cheat.type != CheatType::None) return;
	}

	cheatsLoaded = false;
}

void Cheats::enableCheat(u32 id) {
	if (id < cheats.size()) {
		cheats[id].enabled = true;
	}
}

void Cheats::disableCheat(u32 id) {
	if (id < cheats.size()) {
		cheats[id].enabled = false;
	}
}

void Cheats::clear() {
	cheats.clear();
	cheatsLoaded = false;
}

void Cheats::run() {
	for (const Cheat& cheat : cheats) {
		if (!cheat.enabled) continue;

		switch (cheat.type) {
			case CheatType::ActionReplay: {
				ar.runCheat(cheat.instructions);
				break;
			}

			case CheatType::None: break;
			default: Helpers::panic("Unknown cheat device!");
		}
	}
}