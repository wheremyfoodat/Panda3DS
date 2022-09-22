#pragma once
#include <array>
#include "helpers.hpp"
#include "memory.hpp"
#include "PICA/shader_unit.hpp"

class GPU {
	Memory& mem;
	ShaderUnit shaderUnit;
	u8* vram = nullptr;

	static constexpr u32 maxAttribCount = 12; // Up to 12 vertex attributes
	static constexpr u32 regNum = 0x300;
	static constexpr u32 vramSize = 6_MB;
	std::array<u32, regNum> regs; // GPU internal registers
	
	// Read a value of type T from physical address paddr
	// This is necessary because vertex attribute fetching uses physical addresses
	template<typename T>
	T readPhysical(u32 paddr) {
		if (paddr >= PhysicalAddrs::FCRAM && paddr <= PhysicalAddrs::FCRAMEnd) {
			u8* fcram = mem.getFCRAM();
			u32 index = paddr - PhysicalAddrs::FCRAM;

			return *(T*)&fcram[index];
		} else {
			Helpers::panic("[PICA] Read unimplemented paddr %08X", paddr);
		}
	}

	// Get a pointer of type T* to the data starting from physical address paddr
	template<typename T>
	T* getPointerPhys(u32 paddr) {
		if (paddr >= PhysicalAddrs::FCRAM && paddr <= PhysicalAddrs::FCRAMEnd) {
			u8* fcram = mem.getFCRAM();
			u32 index = paddr - PhysicalAddrs::FCRAM;

			return (T*)&fcram[index];
		}
		else {
			Helpers::panic("[PICA] Pointer to unimplemented paddr %08X", paddr);
		}
	}

	template <bool indexed>
	void drawArrays();

	// Silly method of avoiding linking problems. TODO: Change to something less silly
	void drawArrays(bool indexed);

	struct AttribInfo {
		u32 offset = 0; // Offset from base vertex array
		int size = 0; // Bytes per vertex
		u32 config1 = 0;
		u32 config2 = 0;

		u64 getConfigFull() {
			return u64(config1) | (u64(config2) << 32);
		}
	};

	std::array<AttribInfo, maxAttribCount> attributeInfo; // Info for each of the 12 attributes
	u32 totalAttribCount = 0; // Number of vertex attributes to send to VS
	u32 fixedAttribMask = 0;

public:
	GPU(Memory& mem);
	void clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control);
	void reset();

	// Used by the GSP GPU service for readHwRegs/writeHwRegs/writeHwRegsMasked
	u32 readReg(u32 address);
	void writeReg(u32 address, u32 value);

	// Used when processing GPU command lists
	u32 readInternalReg(u32 index);
	void writeInternalReg(u32 index, u32 value, u32 mask);
};