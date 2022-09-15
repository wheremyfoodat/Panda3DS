#pragma once
#include <vector>
#include "helpers.hpp"

class Memory {
	u8* fcram;

	// Our dynarmic core uses page tables for reads and writes with 4096 byte pages
	std::vector<uintptr_t> readTable, writeTable;
	static constexpr u32 pageShift = 12;
	static constexpr u32 pageSize = 1 << pageShift;
	static constexpr u32 pageMask = pageSize - 1;

public:
	Memory();
	void* getReadPointer(u32 address);
	void* getWritePointer(u32 address);
};