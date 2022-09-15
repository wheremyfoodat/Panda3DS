#pragma once
#include <filesystem>
#include <optional>
#include <vector>
#include "helpers.hpp"

namespace VirtualAddrs {
	enum : u32 {
		ExecutableStart = 0x00100000,
		MaxExeSize = 0x03F00000,
		ExecutableEnd = 0x00100000 + 0x03F00000,

		// Stack for main ARM11 thread.
		// Typically 0x4000 bytes, determined by exheader
		StackTop = 0x10000000,
		StackBottom = 0x0FFFC000,
		StackSize = 0x4000,

		// Start of TLS for first thread. Next thread's storage will be at TLSBase + 0x1000, and so on
		TLSBase = 0xFF400000
	};
}

class Memory {
	u8* fcram;

	// Our dynarmic core uses page tables for reads and writes with 4096 byte pages
	std::vector<uintptr_t> readTable, writeTable;
	static constexpr u32 pageShift = 12;
	static constexpr u32 pageSize = 1 << pageShift;
	static constexpr u32 pageMask = pageSize - 1;
	static constexpr u32 totalPageCount = 1 << (32 - pageShift);

public:
	Memory();
	void* getReadPointer(u32 address);
	void* getWritePointer(u32 address);
	std::optional<u32> loadELF(std::filesystem::path& path);

	u8 read8(u32 vaddr);
	u16 read16(u32 vaddr);
	u32 read32(u32 vaddr);

	void write8(u32 vaddr, u8 value);
	void write16(u32 vaddr, u16 value);
	void write32(u32 vaddr, u32 value);
};