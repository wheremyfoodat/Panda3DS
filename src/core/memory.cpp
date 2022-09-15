#include "memory.hpp"

Memory::Memory() {
	fcram = new uint8_t[128_MB]();
	readTable.resize(totalPageCount, 0);
	writeTable.resize(totalPageCount, 0);

	constexpr u32 fcramPageCount = 128_MB / pageSize;

	// Map 63MB of FCRAM in the executable section as read/write
	// TODO: This should probably be read-only, but making it r/w shouldn't hurt?
	// Because if that were the case then writes would cause data aborts, so games wouldn't write to read-only memory
	u32 executablePageCount = VirtualAddrs::MaxExeSize / pageSize;
	u32 page = VirtualAddrs::ExecutableStart / pageSize;
	for (u32 i = 0; i < executablePageCount; i++) {
		const auto pointer = (uintptr_t)&fcram[i * pageSize];
		readTable[page] = pointer;
		writeTable[page++] = pointer;
	}

	// Map stack pages as R/W
	// We have 16KB for the stack, so we allocate the last 4 pages of FCRAM for the stack
	u32 stackPageCount = VirtualAddrs::StackSize / pageSize;
	u32 fcramStackPage = fcramPageCount - 4;
	page = VirtualAddrs::StackBottom / pageSize;

	for (u32 i = 0; i < 4; i++) {
		auto pointer = (uintptr_t)&fcram[fcramStackPage * pageSize];
		readTable[page] = pointer;
		writeTable[page++] = pointer;
		fcramStackPage++;
	}

	// Map 1 page of FCRAM for thread-local storage
	u32 fcramTLSPage = fcramPageCount - 5;
	page = VirtualAddrs::TLSBase / pageSize;

	auto pointer = (uintptr_t)&fcram[fcramTLSPage * pageSize];
	readTable[page] = pointer;
	writeTable[page++] = pointer;
}

u8 Memory::read8(u32 vaddr) {
	Helpers::panic("Unimplemented 8-bit read, addr: %08X", vaddr);
}

u16 Memory::read16(u32 vaddr) {
	Helpers::panic("Unimplemented 16-bit read, addr: %08X", vaddr);
}

u32 Memory::read32(u32 vaddr) {
	const u32 page = vaddr >> pageShift;
	const u32 offset = vaddr & pageMask;

	uintptr_t pointer = readTable[page];
	if (pointer != 0) [[likely]] {
		return *(u32*)(pointer + offset);
	} else {
		Helpers::panic("Unimplemented 32-bit read, addr: %08X", vaddr);
	}
}

void Memory::write8(u32 vaddr, u8 value) {
	const u32 page = vaddr >> pageShift;
	const u32 offset = vaddr & pageMask;

	uintptr_t pointer = writeTable[page];
	if (pointer != 0) [[likely]] {
		*(u8*)(pointer + offset) = value;
	}
	else {
		Helpers::panic("Unimplemented 8-bit write, addr: %08X, val: %02X", vaddr, value);
	}
}

void Memory::write16(u32 vaddr, u16 value) {
	Helpers::panic("Unimplemented 16-bit write, addr: %08X, val: %04X", vaddr, value);
}

void Memory::write32(u32 vaddr, u32 value) {
	const u32 page = vaddr >> pageShift;
	const u32 offset = vaddr & pageMask;

	uintptr_t pointer = writeTable[page];
	if (pointer != 0) [[likely]] {
		*(u32*)(pointer + offset) = value;
	} else {
		Helpers::panic("Unimplemented 32-bit write, addr: %08X, val: %08X", vaddr, value);
	}
}

void* Memory::getReadPointer(u32 address) {
	const u32 page = address >> pageShift;
	const u32 offset = address & pageMask;

	uintptr_t pointer = readTable[page];
	if (pointer == 0) return nullptr;
	return (void*)(pointer + offset);
}

void* Memory::getWritePointer(u32 address) {
	const u32 page = address >> pageShift;
	const u32 offset = address & pageMask;

	uintptr_t pointer = writeTable[page];
	if (pointer == 0) return nullptr;
	return (void*)(pointer + offset);
}