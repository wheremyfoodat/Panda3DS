#include "gpu.hpp"

u32 GPU::readReg(u32 address) {
	printf("Ignoring read from GPU register %08X\n", address);
	return 0;
}

void GPU::writeReg(u32 address, u32 value) {
	if (address >= 0x1EF01000 && address < 0x1EF01C00) { // Internal registers
		const u32 index = (address - 0x1EF01000) / sizeof(u32);
		writeInternalReg(index, value, 0xffffffff);
	} else {
		Helpers::panic("Ignoring write to GPU register %08X. Value: %08X\n", address, value);
	}
}

u32 GPU::readInternalReg(u32 index) {
	if (index > regNum) {
		printf("Tried to read invalid GPU register. Index: %X\n", index);
		return 0;
	}

	return regs[index];
}

void GPU::writeInternalReg(u32 index, u32 value, u32 mask) {
	if (index > regNum) {
		Helpers::panic("Tried to write to invalid GPU register. Index: %X, value: %08X\n", index, value);
		return;
	}

	u32 currentValue = regs[index];
	u32 newValue = (currentValue & ~mask) | (value & mask); // Only overwrite the bits specified by "mask"

	switch (index) {
		default:
			printf("GPU: Wrote to unimplemented internal reg: %X, value: %08X\n", index, newValue);
			break;
	}
}