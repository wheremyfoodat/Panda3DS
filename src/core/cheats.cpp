#include "cheats.hpp"

Cheats::Cheats(Memory& mem, HIDService& hid) : ar(mem, hid) { reset(); }

void Cheats::reset() {
	clear();     // Clear loaded cheats
	ar.reset();  // Reset ActionReplay
}

void Cheats::addCheat(const Cheat& cheat) {
	cheats.push_back(cheat);
	cheatsLoaded = true;
}

void Cheats::clear() {
	cheats.clear();
	cheatsLoaded = false;
}

void Cheats::run() {
	for (const Cheat& cheat : cheats) {
		switch (cheat.type) {
			case CheatType::ActionReplay: {
				ar.runCheat(cheat.instructions);
				break;
			}

			default: Helpers::panic("Unknown cheat device!");
		}
	}
}