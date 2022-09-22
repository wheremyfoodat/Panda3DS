#include "PICA/gpu.hpp"
#include "PICA/float_types.hpp"
#include "PICA/regs.hpp"
#include <cstdio>

using namespace Floats;

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


	u32* attrBuffer = &regs[0x233];
	auto a = f24::fromRaw(attrBuffer[0] >> 8);
	auto b = f24::fromRaw(((attrBuffer[0] & 0xFF) << 16) | ((attrBuffer[1] >> 16) & 0xFFFF));
	auto g = f24::fromRaw(((attrBuffer[1] & 0xFFFF) << 8) | ((attrBuffer[2] >> 24) & 0xFF));
	auto r = f24::fromRaw(attrBuffer[2] & 0xFFFFFF);

	printf("PICA::DrawArrays(vertex count = %d, vertexOffset = %d)\n", vertexCount, vertexOffset);
	printf("(r: %f, g: %f, b: %f, a: %f)\n", r.toFloat32(), g.toFloat32(), b.toFloat32(), a.toFloat32());
}