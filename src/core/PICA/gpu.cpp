#include "PICA/gpu.hpp"
#include "PICA/float_types.hpp"
#include "PICA/regs.hpp"
#include <cstdio>

using namespace Floats;

GPU::GPU(Memory& mem) : mem(mem) {
	vram = new u8[vramSize];
	totalAttribCount = 0;
	fixedAttribMask = 0;

	for (auto& e : attributeInfo) {
		e.offset = 0;
		e.size = 0;
	}
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

	if constexpr (!indexed) {
		u32 offset = regs[PICAInternalRegs::VertexOffsetReg];
		printf("PICA::DrawArrays(vertex count = %d, vertexOffset = %d)\n", vertexCount, offset);
	} else {
		Helpers::panic("[PICA] Indexed drawing");
	}

	for (u32 i = 0; i < vertexCount; i++) {
		u32 vertexIndex; // Index of the vertex in the VBO
		if constexpr (!indexed) {
			vertexIndex = i + regs[PICAInternalRegs::VertexOffsetReg];
		} else {
			Helpers::panic("[PICA]: Unimplemented indexed rendering");
		}

		int attrCount = 0; // Number of attributes we've passed to the shader
		for (int attrCount = 0; attrCount < totalAttribCount; attrCount++) {
			auto& attr = attributeInfo[attrCount]; // Get information for this attribute
			
			// Check if attribute is fixed or not
			if (fixedAttribMask & (1 << attrCount)) { // Fixed attribute

			} else { // Non-fixed attribute
				// Address to fetch the attribute from
				u32 attrAddress = vertexBase + attr.offset + (vertexIndex * attr.size);
			}
		}
	}
}