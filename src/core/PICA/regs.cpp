#include "PICA/gpu.hpp"
#include "PICA/regs.hpp"

u32 GPU::readReg(u32 address) {
	printf("Ignoring read from GPU register %08X\n", address);
	return 0;
}

void GPU::writeReg(u32 address, u32 value) {
	if (address >= 0x1EF01000 && address < 0x1EF01C00) { // Internal registers
		const u32 index = (address - 0x1EF01000) / sizeof(u32);
		writeInternalReg(index, value, 0xffffffff);
	} else {
		printf("Ignoring write to external GPU register %08X. Value: %08X\n", address, value);
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
	using namespace PICAInternalRegs;

	if (index > regNum) {
		Helpers::panic("Tried to write to invalid GPU register. Index: %X, value: %08X\n", index, value);
		return;
	}

	u32 currentValue = regs[index];
	u32 newValue = (currentValue & ~mask) | (value & mask); // Only overwrite the bits specified by "mask"
	regs[index] = newValue;

	// TODO: Figure out if things like the shader index use the unmasked value or the masked one
	// We currently use the unmasked value like Citra does
	switch (index) {
		case SignalDrawArrays:
			if (value != 0) drawArrays(false);
			break;

		case SignalDrawElements:
			if (value != 0) drawArrays(true);
			break;

		case AttribFormatHigh:
			totalAttribCount = (value >> 28) + 1; // Total number of vertex attributes
			fixedAttribMask = (value >> 16) & 0xfff; // Determines which vertex attributes are fixed for all vertices
			break;

		case VertexShaderTransferEnd:
			if (value != 0) shaderUnit.vs.finalize();
			break;

		case VertexShaderTransferIndex:
			shaderUnit.vs.setBufferIndex(value);
			break;

		case VertexShaderData0: case VertexShaderData1: case VertexShaderData2: case VertexShaderData3:
		case VertexShaderData4: case VertexShaderData5: case VertexShaderData6: case VertexShaderData7:
			shaderUnit.vs.uploadWord(value);
			break;

		default:
			// Vertex attribute registers
			if (index >= AttribInfoStart && index <= AttribInfoEnd) {
				uint attributeIndex = (index - AttribInfoStart) / 3; // Which attribute are we writing to
				uint reg = (index - AttribInfoStart) % 3; // Which of this attribute's registers are we writing to?
				auto& attr = attributeInfo[attributeIndex];

				switch (reg) {
					case 0: attr.offset = value & 0xfffffff; break; // Attribute offset
					case 1: break; // We don't handle this yet
					case 2: // We don't handle most of this yet
						attr.size = (value >> 16) & 0xff;
						break;
				}
			} else {
				printf("GPU: Wrote to unimplemented internal reg: %X, value: %08X\n", index, newValue);
			}
			break;
	}
}