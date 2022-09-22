#include "PICA/gpu.hpp"
#include "PICA/regs.hpp"
#include <cstdio>

void GPU::reset() {
	regs.fill(0);
	shaderUnit.reset();
	// TODO: Reset blending, texturing, etc here
}

void GPU::clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) {
	printf("GPU: Clear buffer\nStart: %08X End: %08X\nValue: %08X Control: %08X\n", startAddress, endAddress, value, control);
}

void GPU::drawArrays() {
	const u32 vertexCount = regs[PICAInternalRegs::VertexCountReg];
	const u32 vertexOffset = regs[PICAInternalRegs::VertexOffsetReg];

	printf("PICA::DrawArrays(vertex count = %d, vertexOffset = %d)\n", vertexCount, vertexOffset);
}