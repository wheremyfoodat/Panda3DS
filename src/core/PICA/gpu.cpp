#include "PICA/gpu.hpp"
#include "PICA/float_types.hpp"
#include "PICA/regs.hpp"
#include <cstdio>

using namespace Floats;

GPU::GPU(Memory& mem) : mem(mem) {
	vram = new u8[vramSize];
}

void GPU::reset() {
	regs.fill(0);
	shaderUnit.reset();
	std::memset(vram, 0, vramSize);
	// TODO: Reset blending, texturing, etc here
}

void GPU::clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) {
	printf("GPU: Clear buffer\nStart: %08X End: %08X\nValue: %08X Control: %08X\n", startAddress, endAddress, value, control);
}

void GPU::drawArrays(bool indexed) {
	if (indexed)
		drawArrays<true>();
	else
		drawArrays<false>();
}

template <bool indexed>
void GPU::drawArrays() {
	// Base address for vertex attributes
	// The vertex base is always on a quadword boundary because the PICA does weird alignment shit any time possible
	const u32 vertexBase = ((regs[PICAInternalRegs::VertexAttribLoc] >> 1) & 0xfffffff) * 16;
	const u32 vertexCount = regs[PICAInternalRegs::VertexCountReg]; // Total # of vertices to transfer
	printf("Vertex location: %08X\n", vertexBase);

	//u32* vertexBuffer = static_cast<u32*>(mem.getReadPointer(vertexBase));
	//if (!vertexBuffer) Helpers::panic("PICA::DrawArrays: Failed to get attribute buffer");

	u32* attrBuffer = &regs[0x233];
	auto a = f24::fromRaw(attrBuffer[0] >> 8);
	auto b = f24::fromRaw(((attrBuffer[0] & 0xFF) << 16) | ((attrBuffer[1] >> 16) & 0xFFFF));
	auto g = f24::fromRaw(((attrBuffer[1] & 0xFFFF) << 8) | ((attrBuffer[2] >> 24) & 0xFF));
	auto r = f24::fromRaw(attrBuffer[2] & 0xFFFFFF);

	if constexpr (!indexed) {
		u32 offset = regs[PICAInternalRegs::VertexOffsetReg];
		printf("PICA::DrawArrays(vertex count = %d, vertexOffset = %d)\n", vertexCount, offset);
	} else {
		Helpers::panic("[PICA] Indexed drawing");
	}

	printf("(r: %f, g: %f, b: %f, a: %f)\n", r.toFloat32(), g.toFloat32(), b.toFloat32(), a.toFloat32());

	for (u32 i = 0; i < vertexCount; i++) {
		u32 vertexIndex; // Index of the vertex in the VBO
		if constexpr (!indexed) {
			vertexIndex = i + regs[PICAInternalRegs::VertexOffsetReg];
		} else {
			Helpers::panic("[PICA]: Unimplemented indexed rendering");
		}

		// Get address of attribute 0
		auto& attr = attributeInfo[0];
		u32 attr0Addr = vertexBase + attr.offset + (vertexIndex * attr.size);
		
		u32 attr0 = readPhysical<u32>(attr0Addr);
		u32 attr0Float = *(float*)&attr0;
		printf("Attr0: %f\n", (double)attr0Float);
	}
}