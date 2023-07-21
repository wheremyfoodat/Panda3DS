#include "cheats.hpp"

Cheats::Cheats(Memory& mem, HIDService& hid) : ar(mem, hid) { reset(); }

void Cheats::reset() {
	cheats.clear();  // Unload loaded cheats
	ar.reset();      // Reset ActionReplay
}

void Cheats::addCheat(const Cheat& cheat) { cheats.push_back(cheat); }

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