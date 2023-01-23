#include "kernel.hpp"
#include "services/shared_font.hpp"

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
// TODO: Does this need to write to outaddr?
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
	
	logSVC("ControlMemory(addr0 = %08X, addr1 = %08X, size = %08X, operation = %X (%c%c%c)%s\n",
			addr0, addr1, size, operation, r ? 'r' : '-', w ? 'w' : '-', x ? 'x' : '-', linear ? ", linear" : ""
	);

	switch (operation & 0xFF) {
		case Operation::Commit: {
			std::optional<u32> address = mem.allocateMemory(addr0, 0, size, linear, r, w, x, true);
			if (!address.has_value())
				Helpers::panic("ControlMemory: Failed to allocate memory");

			regs[1] = address.value();
			break;
		}

		default: Helpers::panic("ControlMemory: unknown operation %X\n", operation);
	}

	regs[0] = SVCResult::Success;
}

// Result QueryMemory(MemoryInfo* memInfo, PageInfo* pageInfo, u32 addr)
void Kernel::queryMemory() {
	const u32 memInfo = regs[0];
	const u32 pageInfo = regs[1];
	const u32 addr = regs[2];

	logSVC("QueryMemory(mem info pointer = %08X, page info pointer = %08X, addr = %08X)\n", memInfo, pageInfo, addr);

	const auto info = mem.queryMemory(addr);
	regs[0] = SVCResult::Success;
	regs[1] = info.baseAddr;
	regs[2] = info.size;
	regs[3] = info.perms;
	regs[4] = info.state;
	regs[5] = 0; // page flags
}

// Result MapMemoryBlock(Handle memblock, u32 addr, MemoryPermission myPermissions, MemoryPermission otherPermission)	
void Kernel::mapMemoryBlock() {
	const Handle block = regs[0];
	u32 addr = regs[1];
	const u32 myPerms = regs[2];
	const u32 otherPerms = regs[3];
	logSVC("MapMemoryBlock(block = %X, addr = %08X, myPerms = %X, otherPerms = %X\n", block, addr, myPerms, otherPerms);

	if (!isAligned(addr)) [[unlikely]] {
		Helpers::panic("MapMemoryBlock: Unaligned address");
	}

	if (KernelHandles::isSharedMemHandle(block)) {
		if (block == KernelHandles::FontSharedMemHandle && addr == 0) addr = 0x18000000;
		u8* ptr = mem.mapSharedMemory(block, addr, myPerms, otherPerms); // Map shared memory block

		// Pass pointer to shared memory to the appropriate service
		switch (block) {
			case KernelHandles::HIDSharedMemHandle:
				serviceManager.setHIDSharedMem(ptr);
				break;

			case KernelHandles::GSPSharedMemHandle:
				serviceManager.setGSPSharedMem(ptr);
				break;

			case KernelHandles::FontSharedMemHandle:
				std::memcpy(ptr, _shared_font_bin, _shared_font_len);
				break;

			default: Helpers::panic("Mapping unknown shared memory block: %X", block);
		}
	} else {
		Helpers::panic("MapMemoryBlock where the handle does not refer to GSP memory");
	}

	regs[0] = SVCResult::Success;
}