#include "action_replay.hpp"

ActionReplay::ActionReplay() { reset(); }

void ActionReplay::reset() {
	// Default value of storage regs is 0
	storage1 = 0;
	storage2 = 0;
}

void ActionReplay::runCheat(const Cheat& cheat) {
	// Set offset and data registers to 0 at the start of a cheat
	data1 = data2 = offset1 = offset2 = 0;
}