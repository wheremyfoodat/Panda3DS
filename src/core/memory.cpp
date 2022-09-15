#include "memory.hpp"

Memory::Memory() {
	fcram = new uint8_t[128_MB]();
}

void* Memory::getReadPointer(u32 address) {
	const auto page = address >> pageShift;
	const auto offset = address & pageMask;

	uintptr_t pointer = readTable[page];
	if (pointer == 0) return nullptr;
	return (void*)(pointer + offset);
}

void* Memory::getWritePointer(u32 address) {
	const auto page = address >> pageShift;
	const auto offset = address & pageMask;

	uintptr_t pointer = writeTable[page];
	if (pointer == 0) return nullptr;
	return (void*)(pointer + offset);
}