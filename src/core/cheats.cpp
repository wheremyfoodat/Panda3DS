#include "cheats.hpp"

#include "swap.hpp"

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

u32 Cheats::addCheat(const u8* data, size_t size) {
	if ((size % 8) != 0) {
		return badCheatHandle;
	}

	Cheats::Cheat cheat;
	cheat.enabled = true;
	cheat.type = Cheats::CheatType::ActionReplay;

	for (size_t i = 0; i < size; i += 8) {
		auto read32 = [](const u8* ptr) { return (u32(ptr[3]) << 24) | (u32(ptr[2]) << 16) | (u32(ptr[1]) << 8) | u32(ptr[0]); };

		// Data is passed to us in big endian so we bswap
		u32 firstWord = Common::swap32(read32(data + i));
		u32 secondWord = Common::swap32(read32(data + i + 4));
		cheat.instructions.insert(cheat.instructions.end(), {firstWord, secondWord});
	}

	return addCheat(cheat);
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