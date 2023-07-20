#include "action_replay.hpp"

ActionReplay::ActionReplay(Memory& mem) : mem(mem) { reset(); }

void ActionReplay::reset() {
	// Default value of storage regs is 0
	storage1 = 0;
	storage2 = 0;
}

void ActionReplay::runCheat(const Cheat& cheat) {
	// Set offset and data registers to 0 at the start of a cheat
	data1 = data2 = offset1 = offset2 = 0;
	pc = 0;
	running = true;

	activeOffset = &offset1;
	activeData = &data1;

	while (running) {
		// See if we can fetch 1 64-bit opcode, otherwise we're out of bounds. Cheats seem to end when going out of bounds?
		if (pc + 1 >= cheat.size()) {
			return;
		}

		// Fetch instruction
		const u32 instruction = cheat[pc++];
		runInstruction(cheat, instruction);
	}
}

u8 ActionReplay::read8(u32 addr) { return mem.read8(addr); }
u16 ActionReplay::read16(u32 addr) { return mem.read16(addr); }
u32 ActionReplay::read32(u32 addr) { return mem.read32(addr); }

void ActionReplay::write8(u32 addr, u8 value) { mem.write8(addr, value); }
void ActionReplay::write16(u32 addr, u16 value) { mem.write16(addr, value); }
void ActionReplay::write32(u32 addr, u32 value) { mem.write32(addr, value); }

void ActionReplay::runInstruction(const Cheat& cheat, u32 instruction) {
	// Top nibble determines the instruction type
	const u32 type = instruction >> 28;

	switch (type) {
		// 16-bit write to [XXXXXXX + offset]
		case 0x1: {
			const u32 baseAddr = Helpers::getBits<0, 28>(instruction);
			const u16 value = u16(cheat[pc++]);
			write16(baseAddr + *activeOffset, value);
			break;
		}


		// 8-bit write to [XXXXXXX + offset]
		case 0x2: {
			const u32 baseAddr = Helpers::getBits<0, 28>(instruction);
			const u8 value = u8(cheat[pc++]);
			write8(baseAddr + *activeOffset, value);
			break;
		}

		// Less Than (YYYYYYYY < [XXXXXXX + offset])
		case 0x4: {
			const u32 baseAddr = Helpers::getBits<0, 28>(instruction);
			const u32 imm = cheat[pc++];
			const u32 value = read32(baseAddr + *activeOffset);
			Helpers::panic("TODO: How do ActionReplay conditional blocks work?");
			break;
		}

		case 0xD: executeDType(cheat, instruction); break;
		default: Helpers::panic("Unimplemented ActionReplay instruction type %X", type); break;
	}
}

void ActionReplay::executeDType(const Cheat& cheat, u32 instruction) {
	Helpers::panic("ActionReplay: Unimplemented d-type opcode: %08X", instruction);
}