#pragma once
#include <array>
#include <vector>

#include "helpers.hpp"
#include "memory.hpp"

class ActionReplay {
	using Cheat = std::vector<u32>;  // A cheat is really just a bunch of 64-bit opcodes neatly encoded into 32-bit chunks

	u32 offset1, offset2;    // Memory offset registers. Non-persistent.
	u32 data1, data2;        // Data offset registers. Non-persistent.
	u32 storage1, storage2;  // Storage registers. Persistent.

	// When an instruction does not specify which offset or data register to use, we use the "active" one
	// Which is by default #1 and may be changed by certain AR operations
	u32 *activeOffset, *activeData;
	
	// Program counter
	u32 pc = 0;
	Memory& mem;

	// Has the cheat ended?
	bool running = false;
	// Run 1 AR instruction
	void runInstruction(const Cheat& cheat, u32 instruction);

	// Action Replay has a billion D-type opcodes so this handles all of them
	void executeDType(const Cheat& cheat, u32 instruction);

	u8 read8(u32 addr);
	u16 read16(u32 addr);
	u32 read32(u32 addr);

	void write8(u32 addr, u8 value);
	void write16(u32 addr, u16 value);
	void write32(u32 addr, u32 value);

  public:
	ActionReplay(Memory& mem);
	void runCheat(const Cheat& cheat);
	void reset();
};