#include "action_replay.hpp"

ActionReplay::ActionReplay(Memory& mem, HIDService& hid) : mem(mem), hid(hid) { reset(); }

void ActionReplay::reset() {
	// Default value of storage regs is 0
	storage1 = 0;
	storage2 = 0;

	// TODO: Is the active storage persistent or not?
	activeStorage = &storage1;
}

void ActionReplay::runCheat(const Cheat& cheat) {
	// Set offset and data registers to 0 at the start of a cheat
	data1 = data2 = offset1 = offset2 = 0;
	pc = 0;
	ifStackIndex = 0;
	loopStackIndex = 0;
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

		// Instructions D0000000 00000000 and D2000000 00000000 are unconditional
		bool isUnconditional = cheat[pc] == 0 && (instruction == 0xD0000000 || instruction == 0xD2000000);
		if (ifStackIndex > 0 && !isUnconditional && !ifStack[ifStackIndex - 1]) {
			pc++;      // Eat up dummy word
			continue;  // Skip conditional instructions where the condition is false
		}

		runInstruction(cheat, instruction);
	}
}

u8 ActionReplay::read8(u32 addr) { return mem.read8(addr); }
u16 ActionReplay::read16(u32 addr) { return mem.read16(addr); }
u32 ActionReplay::read32(u32 addr) { return mem.read32(addr); }

// Some AR cheats seem to want to write to unmapped memory or memory that straight up does not exist

#define MAKE_WRITE_HANDLER(size)                                                          \
	void ActionReplay::write##size(u32 addr, u##size value) {                             \
		auto pointerWrite = mem.getWritePointer(addr);                                    \
		if (pointerWrite) {                                                               \
			*(u##size*)pointerWrite = value;                                              \
		} else {                                                                          \
			auto pointerRead = mem.getReadPointer(addr);                                  \
			if (pointerRead) {                                                            \
				*(u##size*)pointerRead = value;                                           \
			} else {                                                                      \
				Helpers::warn("AR code tried to write to invalid address: %08X\n", addr); \
			}                                                                             \
		}                                                                                 \
	}

MAKE_WRITE_HANDLER(8)
MAKE_WRITE_HANDLER(16)
MAKE_WRITE_HANDLER(32)
#undef MAKE_WRITE_HANDLER

void ActionReplay::runInstruction(const Cheat& cheat, u32 instruction) {
	// Top nibble determines the instruction type
	const u32 type = instruction >> 28;

	switch (type) {
		// 32-bit write to [XXXXXXX + offset]
		case 0x0: {
			const u32 baseAddr = Helpers::getBits<0, 28>(instruction);
			const u32 value = cheat[pc++];
			write32(baseAddr + *activeOffset, value);
			break;
		}

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

		// clang-format off
		#define MAKE_IF_INSTRUCTION(opcode, comparator)                \
		case opcode: {                                                 \
			const u32 baseAddr = Helpers::getBits<0, 28>(instruction); \
			const u32 imm = cheat[pc++];                               \
			const u32 value = read32(baseAddr + *activeOffset);        \
				                                                       \
			pushConditionBlock(imm comparator value);                  \
			break;                                                     \
		}

		// Greater Than (YYYYYYYY > [XXXXXXX + offset]) (Unsigned)
		MAKE_IF_INSTRUCTION(3, >)

		// Less Than (YYYYYYYY < [XXXXXXX + offset]) (Unsigned)
		MAKE_IF_INSTRUCTION(4, <)

		// Equal to (YYYYYYYY == [XXXXXXX + offset])
		MAKE_IF_INSTRUCTION(5, ==)

		// Not Equal (YYYYYYYY != [XXXXXXX + offset])
		MAKE_IF_INSTRUCTION(6, !=)
		#undef MAKE_IF_INSTRUCTION
		// clang-format on

		// BXXXXXXX 00000000 - offset = *(XXXXXXX + offset)
		case 0xB: {
			const u32 baseAddr = Helpers::getBits<0, 28>(instruction);
			*activeOffset = read32(baseAddr + *activeOffset);

			pc++;  // Eat up dummy word
			break;
		}

		case 0xD: executeDType(cheat, instruction); break;
		default: Helpers::panic("Unimplemented ActionReplay instruction type %X", type); break;
	}
}

void ActionReplay::executeDType(const Cheat& cheat, u32 instruction) {
	switch (instruction) {
		case 0xD3000000: offset1 = cheat[pc++]; break;
		case 0xD3000001: offset2 = cheat[pc++]; break;

		case 0xD6000000:
			write32(*activeOffset + cheat[pc++], u32(*activeData));
			*activeOffset += 4;
			break;

		case 0xD6000001:
			write32(*activeOffset + cheat[pc++], u32(data1));
			*activeOffset += 4;
			break;

		case 0xD6000002:
			write32(*activeOffset + cheat[pc++], u32(data2));
			*activeOffset += 4;
			break;

		case 0xD7000000:
			write16(*activeOffset + cheat[pc++], u16(*activeData));
			*activeOffset += 2;
			break;

		case 0xD7000001:
			write16(*activeOffset + cheat[pc++], u16(data1));
			*activeOffset += 2;
			break;

		case 0xD7000002:
			write16(*activeOffset + cheat[pc++], u16(data2));
			*activeOffset += 2;
			break;

		case 0xD8000000:
			write8(*activeOffset + cheat[pc++], u8(*activeData));
			*activeOffset += 1;
			break;

		case 0xD8000001:
			write8(*activeOffset + cheat[pc++], u8(data1));
			*activeOffset += 1;
			break;

		case 0xD8000002:
			write8(*activeOffset + cheat[pc++], u8(data2));
			*activeOffset += 1;
			break;

		case 0xD9000000: *activeData = read32(cheat[pc++] + *activeOffset); break;
		case 0xD9000001: data1 = read32(cheat[pc++] + *activeOffset); break;
		case 0xD9000002: data2 = read32(cheat[pc++] + *activeOffset); break;

		case 0xDA000000: *activeData = read16(cheat[pc++] + *activeOffset); break;
		case 0xDA000001: data1 = read16(cheat[pc++] + *activeOffset); break;
		case 0xDA000002: data2 = read16(cheat[pc++] + *activeOffset); break;

		case 0xDB000000: *activeData = read8(cheat[pc++] + *activeOffset); break;
		case 0xDB000001: data1 = read8(cheat[pc++] + *activeOffset); break;
		case 0xDB000002: data2 = read8(cheat[pc++] + *activeOffset); break;

		case 0xDC000000: *activeOffset += cheat[pc++]; break;

		// DD000000 XXXXXXXX - if KEYPAD has value XXXXXXXX execute next block
		case 0xDD000000: {
			const u32 mask = cheat[pc++];
			const u32 buttons = hid.getOldButtons();

			pushConditionBlock((buttons & mask) == mask);
			break;
		}

		// Offset register ops
		case 0xDF000000: {
			const u32 subopcode = cheat[pc++];
			switch (subopcode) {
				case 0x00000000: activeOffset = &offset1; break;
				case 0x00000001: activeOffset = &offset2; break;
				case 0x00010000: offset2 = offset1; break;
				case 0x00010001: offset1 = offset2; break;
				case 0x00020000: data1 = offset1; break;
				case 0x00020001: data2 = offset2; break;
				default:
					Helpers::warn("Unknown ActionReplay offset operation");
					running = false;
					break;
			}
			break;
		}

		// Data register operations
		case 0xDF000001: {
			const u32 subopcode = cheat[pc++];
			switch (subopcode) {
				case 0x00000000: activeData = &data1; break;
				case 0x00000001: activeData = &data2; break;

				case 0x00010000: data2 = data1; break;
				case 0x00010001: data1 = data2; break;
				case 0x00020000: offset1 = data1; break;
				case 0x00020001: offset2 = data2; break;
				default:
					Helpers::warn("Unknown ActionReplay data operation");
					running = false;
					break;
			}
			break;
		}

		// Storage register operations
		case 0xDF000002: {
			const u32 subopcode = cheat[pc++];
			switch (subopcode) {
				case 0x00000000: activeStorage = &storage1; break;
				case 0x00000001: activeStorage = &storage2; break;

				case 0x00010000: data1 = storage1; break;
				case 0x00010001: data2 = storage2; break;
				case 0x00020000: storage1 = data1; break;
				case 0x00020001: storage2 = data2; break;
				default:
					Helpers::warn("Unknown ActionReplay data operation: %08X", subopcode);
					running = false;
					break;
			}
			break;
		}

		// Control flow block operations
		case 0xD2000000: {
			const u32 subopcode = cheat[pc++];
			switch (subopcode) {
				// Ends all loop/execute blocks
				case 0:
					loopStackIndex = 0;
					ifStackIndex = 0;
					break;
				default: Helpers::panic("Unknown ActionReplay control flow operation: %08X", subopcode); break;
			}
			break;
		}

		default: Helpers::panic("ActionReplay: Unimplemented d-type opcode: %08X", instruction); break;
	}
}

void ActionReplay::pushConditionBlock(bool condition) {
	if (ifStackIndex >= 32) {
		Helpers::warn("ActionReplay if stack overflowed");
		running = false;
		return;
	}

	ifStack[ifStackIndex++] = condition;
}