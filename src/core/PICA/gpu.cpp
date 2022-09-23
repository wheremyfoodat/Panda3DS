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

	totalAttribCount = 0;
	fixedAttribMask = 0;
	fixedAttribIndex = 0;
	fixedAttribCount = 0;
	fixedAttrBuff.fill(0);

	for (auto& e : attributeInfo) {
		e.offset = 0;
		e.size = 0;
		e.config1 = 0;
		e.config2 = 0;
	}

	// TODO: Reset blending, texturing, etc here
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

	// Configures the type of primitive and the number of vertex shader outputs
	const u32 primConfig = regs[PICAInternalRegs::PrimitiveConfig];
	const u32 primType = (primConfig >> 8) & 3;
	if (primType != 0) Helpers::panic("[PICA] Tried to draw non-triangle shape %d\n", primType);
	if (vertexCount % 3) Helpers::panic("[PICA] Vertex count not a multiple of 3");
	if (vertexCount > vertexBufferSize) Helpers::panic("[PICA] vertexCount > vertexBufferSize");

	Vertex vertices[vertexBufferSize];

	// Stuff the global attribute config registers in one u64 to make attr parsing easier
	// TODO: Cache this when the vertex attribute format registers are written to 
	u64 vertexCfg = u64(regs[PICAInternalRegs::AttribFormatLow]) | (u64(regs[PICAInternalRegs::AttribFormatHigh]) << 32);

	if constexpr (!indexed) {
		u32 offset = regs[PICAInternalRegs::VertexOffsetReg];
		log("PICA::DrawArrays(vertex count = %d, vertexOffset = %d)\n", vertexCount, offset);
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
			// Check if attribute is fixed or not
			if (fixedAttribMask & (1 << attrCount)) { // Fixed attribute
				vec4f& fixedAttr = shaderUnit.vs.fixedAttributes[attrCount]; // TODO: Is this how it works?
				vec4f& inputAttr = shaderUnit.vs.attributes[attrCount];
				std::memcpy(&inputAttr, &fixedAttr, sizeof(vec4f)); // Copy fixed attr to input attr
			} else { // Non-fixed attribute
				auto& attr = attributeInfo[attrCount]; // Get information for this attribute
				u64 attrCfg = attr.getConfigFull(); // Get config1 | (config2 << 32)
				uint index = (attrCfg >> (attrCount * 4)) & 0xf; // Get index of attribute in vertexCfg

				if (index >= 12) Helpers::panic("[PICA] Vertex attribute used as padding");

				u32 attribInfo = (vertexCfg >> (index * 4)) & 0xf;
				u32 attribType = attribInfo & 0x3; //  Type of attribute(sbyte/ubyte/short/float)
				u32 componentCount = (attribInfo >> 2) + 1; // Total number of components

				// Address to fetch the attribute from
				u32 attrAddress = vertexBase + attr.offset + (vertexIndex * attr.size);
				vec4f& attribute = shaderUnit.vs.attributes[attrCount];
				uint component; // Current component

				switch (attribType) {
					case 3: { // Float
						float* ptr = getPointerPhys<float>(attrAddress);
						for (component = 0; component < componentCount; component++) {
							float val = *ptr++;
							attribute[component] = f24::fromFloat32(val);
						}
						break;
					}

					default: Helpers::panic("[PICA] Unimplemented component type %d", attribType);
				}

				// Fill the remaining attribute lanes with default parameters (1.0 for alpha/w, 0.0) for everything else
				// Corgi does this although I'm not sure if it's actually needed for anything.
				// TODO: Find out
				while (component < 4) {
					attribute[component] = (component == 3) ? f24::fromFloat32(1.0) : f24::fromFloat32(0.0);
					component++;
				}
			}
		}

		shaderUnit.vs.run();
		std::memcpy(&vertices[i].position, &shaderUnit.vs.outputs[0], sizeof(vec4f));
		std::memcpy(&vertices[i].colour, &shaderUnit.vs.outputs[1], sizeof(vec4f));
	}

	drawVertices(OpenGL::Triangle, vertices, vertexCount);
}