#include "PICA/draw_acceleration.hpp"

#include <limits>

#include "PICA/gpu.hpp"
#include "PICA/regs.hpp"

void GPU::getAcceleratedDrawInfo(PICA::DrawAcceleration& accel, bool indexed) {
	accel.indexed = indexed;
	accel.totalAttribCount = totalAttribCount;

	const u32 vertexBase = ((regs[PICA::InternalRegs::VertexAttribLoc] >> 1) & 0xfffffff) * 16;
	const u32 vertexCount = regs[PICA::InternalRegs::VertexCountReg];  // Total # of vertices to transfer

	accel.vertexBuffer = getPointerPhys<u8>(vertexBase);
	if (indexed) {
		u32 indexBufferConfig = regs[PICA::InternalRegs::IndexBufferConfig];
		u32 indexBufferPointer = vertexBase + (indexBufferConfig & 0xfffffff);

		u8* indexBuffer = getPointerPhys<u8>(indexBufferPointer);
		u16 minimumIndex = std::numeric_limits<u16>::max();
		u16 maximumIndex = 0;

		// Check whether the index buffer uses u16 indices or u8
		bool shortIndex = Helpers::getBit<31>(indexBufferConfig);  // Indicates whether vert indices are 16-bit or 8-bit

		// Calculate the minimum and maximum indices used in the index buffer, so we'll only upload them
		if (shortIndex) {
			u16* indexBuffer16 = reinterpret_cast<u16*>(indexBuffer);
			for (int i = 0; i < vertexCount; i++) {
				u16 index = indexBuffer16[i];
				minimumIndex = std::min(minimumIndex, index);
				maximumIndex = std::max(maximumIndex, index);
			}
		} else {
			for (int i = 0; i < vertexCount; i++) {
				u16 index = u16(indexBuffer[i]);
				minimumIndex = std::min(minimumIndex, index);
				maximumIndex = std::max(maximumIndex, index);
			}
		}

		accel.indexBuffer = indexBuffer;
		accel.minimumIndex = minimumIndex;
		accel.maximumIndex = maximumIndex;
	} else {
		accel.indexBuffer = nullptr;
		accel.minimumIndex = regs[PICA::InternalRegs::VertexOffsetReg];
		accel.maximumIndex = accel.minimumIndex + vertexCount - 1;
	}

	const u64 vertexCfg = u64(regs[PICA::InternalRegs::AttribFormatLow]) | (u64(regs[PICA::InternalRegs::AttribFormatHigh]) << 32);
	u32 buffer = 0;
	u32 attrCount = 0;
	accel.vertexDataSize = 0;

	while (attrCount < totalAttribCount) {
		bool fixedAttrib = (fixedAttribMask & (1 << attrCount)) != 0;

		// Variable attribute attribute
		if (!fixedAttrib) {
			auto& attrData = attributeInfo[buffer];  // Get information for this attribute
			u64 attrCfg = attrData.getConfigFull();  // Get config1 | (config2 << 32)
			u32 attributeOffset = attrData.offset;

			if (attrData.componentCount != 0) {
				// Size of the attribute in bytes multiplied by the total number of vertices
				const u32 bytes = attrData.size * vertexCount;
				// Add it to the total vertex data size, aligned to 4 bytes.
				accel.vertexDataSize += (bytes + 3) & ~3;
			}

			for (int i = 0; i < attrData.componentCount; i++) {
				uint index = (attrCfg >> (i * 4)) & 0xf;  // Get index of attribute in vertexCfg
				auto& attr = accel.attributeInfo[attrCount];
				attr.fixed = false;

				// Vertex attributes used as padding
				// 12, 13, 14 and 15 are equivalent to 4, 8, 12 and 16 bytes of padding respectively
				if (index >= 12) [[unlikely]] {
					Helpers::panic("Padding attribute");
					// Align attribute address up to a 4 byte boundary
					attributeOffset = (attributeOffset + 3) & -4;
					attributeOffset += (index - 11) << 2;

					attr.data = nullptr;
					continue;
				}

				const u32 attribInfo = (vertexCfg >> (index * 4)) & 0xf;
				const u32 attribType = attribInfo & 0x3;  //  Type of attribute (sbyte/ubyte/short/float)
				const u32 size = (attribInfo >> 2) + 1;   // Total number of components
			
				attr.componentCount = size;
				attr.offset = attributeOffset;
				attr.type = attribType;

				// Get a pointer to the data where this attribute is stored
				const u32 attrAddress = vertexBase + attr.offset + (accel.minimumIndex * attrData.size);
				attr.data = getPointerPhys<u8>(attrAddress);

				// Size of each component based on the attribute type
				static constexpr u32 sizePerComponent[4] = {1, 1, 2, 4};
				attributeOffset += size * sizePerComponent[attribType];

				attrCount += 1;
			}

			buffer += 1;
		} else {
			vec4f& fixedAttr = shaderUnit.vs.fixedAttributes[attrCount];
			auto& attr = accel.attributeInfo[attrCount];

			attr.fixed = true;
			// Set the data pointer to nullptr in order to catch any potential bugs
			attr.data = nullptr;

			for (int i = 0; i < 4; i++) {
				attr.fixedValue[i] = fixedAttr[i].toFloat32();
			}

			attrCount += 1;
		}
	}

	accel.canBeAccelerated = true;
}