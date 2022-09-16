#include "kernel.hpp"

namespace Operation {
	enum : u32 {
		Free = 1,
		Reserve = 2,
		Commit = 3,
		Map = 4,
		Unmap = 5,
		Protect = 6,
		AppRegion = 0x100,
		SysRegion = 0x200,
		BaseRegion = 0x300,
		Linear = 0x10000
	};
}

// Returns whether "value" is aligned to a page boundary (Ie a boundary of 4096 bytes)
static constexpr bool isAligned(u32 value) {
	return (value & 0xFFF) == 0;
}

// Result ControlMemory(u32* outaddr, u32 addr0, u32 addr1, u32 size, 
//						MemoryOperation operation, MemoryPermission permissions)
// This has a weird ABI documented here https://www.3dbrew.org/wiki/Kernel_ABI
void Kernel::controlMemory() {
	u32 operation = regs[0]; // The base address is written here
	u32 addr0 = regs[1];
	u32 addr1 = regs[2];
	u32 size = regs[3];
	u32 perms = regs[4];

	if (perms == 0x10000000) {
		perms = 3; // We make "don't care" equivalent to read-write
		Helpers::panic("Unimplemented allocation permission: DONTCARE");
	}

	// Naturally the bits are in reverse order
	bool r = perms & 0b001;
	bool w = perms & 0b010;
	bool x = perms & 0b100;
	bool linear = operation & Operation::Linear;

	if (x)
		Helpers::panic("ControlMemory: attempted to allocate executable memory");

	if (!isAligned(addr0) || !isAligned(addr1) || !isAligned(size)) {
		Helpers::panic("ControlMemory: Unaligned parameters\nAddr0: %08X\nAddr1: %08X\nSize: %08X", addr0, addr1, size);
	}
	
	printf("ControlMemory(addr0 = %08X, addr1 = %08X, size = %X, operation = %X (%c%c%c)%s\n",
			addr0, addr1, size, operation, r ? 'r' : '-', w ? 'w' : '-', x ? 'x' : '-', linear ? ", linear" : ""
	);

	switch (operation & 0xFF) {
		case Operation::Commit:
			break;

		default: Helpers::panic("ControlMemory: unknown operation %X\n", operation);
	}

	regs[0] = SVCResult::Success;
	regs[1] = 0xF3180131;
}