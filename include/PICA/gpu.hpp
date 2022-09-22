#pragma once
#include <array>
#include "helpers.hpp"
#include "memory.hpp"
#include "PICA/shader_unit.hpp"

class GPU {
	Memory& mem;
	static constexpr u32 regNum = 0x300;
	ShaderUnit shaderUnit;
	std::array<u32, regNum> regs; // GPU internal registers

	void drawArrays();

public:
	GPU(Memory& mem) : mem(mem) {}
	void clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control);
	void reset();

	// Used by the GSP GPU service for readHwRegs/writeHwRegs/writeHwRegsMasked
	u32 readReg(u32 address);
	void writeReg(u32 address, u32 value);

	// Used when processing GPU command lists
	u32 readInternalReg(u32 index);
	void writeInternalReg(u32 index, u32 value, u32 mask);
};