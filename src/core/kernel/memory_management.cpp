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

namespace MemoryPermissions {
	enum : u32 {
		None = 0,             // ---
		Read = 1,             // R--
		Write = 2,            // -W-
		ReadWrite = 3,        // RW-
		Execute = 4,          // --X
		ReadExecute = 5,      // R-X
		WriteExecute = 6,     // -WX
		ReadWriteExecute = 7, // RWX

		DontCare = 0x10000000
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

	if (perms == MemoryPermissions::DontCare) {
		perms = MemoryPermissions::ReadWrite; // We make "don't care" equivalent to read-write
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

		case Operation::Map:
			mem.mirrorMapping(addr0, addr1, size);
			break;

		case Operation::Protect:
			Helpers::warn("Ignoring mprotect! Hope nothing goes wrong but if the game accesses invalid memory or crashes then we prolly need to implement this\n");
			break;

		default: Helpers::warn("ControlMemory: unknown operation %X\n", operation); break;
	}

	regs[0] = Result::Success;
}

// Result QueryMemory(MemoryInfo* memInfo, PageInfo* pageInfo, u32 addr)
void Kernel::queryMemory() {
	const u32 memInfo = regs[0];
	const u32 pageInfo = regs[1];
	const u32 addr = regs[2];

	logSVC("QueryMemory(mem info pointer = %08X, page info pointer = %08X, addr = %08X)\n", memInfo, pageInfo, addr);

	const auto info = mem.queryMemory(addr);
	regs[0] = Result::Success;
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
				mem.copySharedFont(ptr);
				break;

			default: Helpers::panic("Mapping unknown shared memory block: %X", block);
		}
	} else {
		Helpers::panic("MapMemoryBlock where the handle does not refer to a known piece of kernel shared mem");
	}

	regs[0] = Result::Success;
}

Handle Kernel::makeMemoryBlock(u32 addr, u32 size, u32 myPermission, u32 otherPermission) {
	Handle ret = makeObject(KernelObjectType::MemoryBlock);
	objects[ret].data = new MemoryBlock(addr, size, myPermission, otherPermission);

	return ret;
}

void Kernel::createMemoryBlock() {
	const u32 addr = regs[1];
	const u32 size = regs[2];
	u32 myPermission = regs[3];
	u32 otherPermission = mem.read32(regs[13] + 4); // This is placed on the stack rather than r4
	logSVC("CreateMemoryBlock (addr = %08X, size = %08X, myPermission = %d, otherPermission = %d)\n", addr, size, myPermission, otherPermission);

	// Returns whether a permission is valid
	auto isPermValid = [](u32 permission) {
		switch (permission) {
			case MemoryPermissions::None:
			case MemoryPermissions::Read:
			case MemoryPermissions::Write:
			case MemoryPermissions::ReadWrite:
			case MemoryPermissions::DontCare:
				return true;

			default: // Permissions with the executable flag enabled or invalid permissions are not allowed
				return false;
		}
	};

	// Throw error if the size of the shared memory block is not aligned to page boundary
	if (!isAligned(size)) {
		regs[0] = Result::OS::MisalignedSize;
		return;
	}

	// Throw error if one of the permissions is not valid
	if (!isPermValid(myPermission) || !isPermValid(otherPermission)) {
		regs[0] = Result::OS::InvalidCombination;
		return;
	}

	// TODO: The address needs to be in a specific range otherwise it throws an invalid address error

	if (addr == 0)
		Helpers::panic("CreateMemoryBlock: Tried to use addr = 0");

	// Implement "Don't care" permission as RW
	if (myPermission == MemoryPermissions::DontCare) myPermission = MemoryPermissions::ReadWrite;
	if (otherPermission == MemoryPermissions::DontCare) otherPermission = MemoryPermissions::ReadWrite;

	regs[0] = Result::Success;
	regs[1] = makeMemoryBlock(addr, size, myPermission, otherPermission);
}
