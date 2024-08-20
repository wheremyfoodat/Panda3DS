#include "PICA/draw_acceleration.hpp"

#include <limits>

#include "PICA/gpu.hpp"
#include "PICA/regs.hpp"

void GPU::getAcceleratedDrawInfo(PICA::DrawAcceleration& accel, bool indexed) {
	accel.indexed = indexed;
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

	int buffer = 0;
	accel.vertexDataSize = 0;

	for (int attrCount = 0; attrCount < totalAttribCount; attrCount++) {
		bool fixedAttribute = (fixedAttribMask & (1 << attrCount)) != 0;

		if (!fixedAttribute) {
			auto& attr = attributeInfo[buffer];  // Get information for this attribute
			
			if (attr.componentCount != 0) {
				// Size of the attribute in bytes multiplied by the total number of vertices
				const u32 bytes = attr.size * vertexCount;
				// Add it to the total vertex data size, aligned to 4 bytes.
				accel.vertexDataSize += (bytes + 3) & ~3;
			}

			buffer++;
		}
	}

	accel.canBeAccelerated = true;
}