#pragma once
#include <array>
#include <vector>

#include "helpers.hpp"

class ActionReplay {
	using Cheat = std::vector<u32>;  // A cheat is really just a bunch of u32 opcodes

	u32 offset1, offset2;    // Memory offset registers. Non-persistent.
	u32 data1, data2;        // Data offset registers. Non-persistent.
	u32 storage1, storage2;  // Storage registers. Persistent.

	// When an instruction does not specify which offset or data register to use, we use the "active" one
	// Which is by default #1 and may be changed by certain AR operations
	u32 *activeOffset, *activeData;

  public:
	ActionReplay();
	void runCheat(const Cheat& cheat);
	void reset();
};