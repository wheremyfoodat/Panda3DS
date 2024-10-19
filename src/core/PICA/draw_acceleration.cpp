#include "PICA/draw_acceleration.hpp"

#include <bit>
#include <limits>

#include "PICA/gpu.hpp"
#include "PICA/regs.hpp"

void GPU::getAcceleratedDrawInfo(PICA::DrawAcceleration& accel, bool indexed) {
	accel.indexed = indexed;
	accel.totalAttribCount = totalAttribCount;
	accel.enabledAttributeMask = 0;
	
	const u32 vertexBase = ((regs[PICA::InternalRegs::VertexAttribLoc] >> 1) & 0xfffffff) * 16;
	const u32 vertexCount = regs[PICA::InternalRegs::VertexCountReg];  // Total # of vertices to transfer

	if (indexed) {
		u32 indexBufferConfig = regs[PICA::InternalRegs::IndexBufferConfig];
		u32 indexBufferPointer = vertexBase + (indexBufferConfig & 0xfffffff);

		u8* indexBuffer = getPointerPhys<u8>(indexBufferPointer);
		u16 minimumIndex = std::numeric_limits<u16>::max();
		u16 maximumIndex = 0;

		// Check whether the index buffer uses u16 indices or u8
		accel.useShortIndices = Helpers::getBit<31>(indexBufferConfig);  // Indicates whether vert indices are 16-bit or 8-bit

		// Calculate the minimum and maximum indices used in the index buffer, so we'll only upload them
		if (accel.useShortIndices) {
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
	const u64 inputAttrCfg = getVertexShaderInputConfig();

	u32 attrCount = 0;
	u32 loaderOffset = 0;
	accel.vertexDataSize = 0;
	accel.totalLoaderCount = 0;

	for (int i = 0; i < PICA::DrawAcceleration::maxLoaderCount; i++) {
		auto& loaderData = attributeInfo[i];  // Get information for this attribute loader

		// This loader is empty, skip it
		if (loaderData.componentCount == 0 || loaderData.size == 0) {
			continue;
		}

		auto& loader = accel.loaders[accel.totalLoaderCount++];

		// The size of the loader in bytes is equal to the bytes supplied for 1 vertex, multiplied by the number of vertices we'll be uploading
		// Which is equal to maximumIndex - minimumIndex + 1
		const u32 bytes = loaderData.size * (accel.maximumIndex - accel.minimumIndex + 1);
		loader.size = bytes;

		// Add it to the total vertex data size, aligned to 4 bytes.
		accel.vertexDataSize += (bytes + 3) & ~3;
		
		// Get a pointer to the data where this loader's data is stored
		const u32 loaderAddress = vertexBase + loaderData.offset + (accel.minimumIndex * loaderData.size);
		loader.data = getPointerPhys<u8>(loaderAddress);

		u64 attrCfg = loaderData.getConfigFull();  // Get config1 | (config2 << 32)
		u32 attributeOffset = 0;

		for (int component = 0; component < loaderData.componentCount; component++) {
			uint attributeIndex = (attrCfg >> (component * 4)) & 0xf;  // Get index of attribute in vertexCfg

			// Vertex attributes used as padding
			// 12, 13, 14 and 15 are equivalent to 4, 8, 12 and 16 bytes of padding respectively
			if (attributeIndex >= 12) [[unlikely]] {
				// Align attribute address up to a 4 byte boundary
				attributeOffset = (attributeOffset + 3) & -4;
				attributeOffset += (attributeIndex - 11) << 2;
				continue;
			}

			const u32 attribInfo = (vertexCfg >> (attributeIndex * 4)) & 0xf;
			const u32 attribType = attribInfo & 0x3;  //  Type of attribute (sbyte/ubyte/short/float)
			const u32 size = (attribInfo >> 2) + 1;   // Total number of components

			// Size of each component based on the attribute type
			static constexpr u32 sizePerComponent[4] = {1, 1, 2, 4};
			const u32 inputReg = (inputAttrCfg >> (attributeIndex * 4)) & 0xf;
			// Mark the attribute as enabled
			accel.enabledAttributeMask |= 1 << inputReg;

			auto& attr = accel.attributeInfo[inputReg];
			attr.componentCount = size;
			attr.offset = attributeOffset + loaderOffset;
			attr.stride = loaderData.size;
			attr.type = attribType;
			attributeOffset += size * sizePerComponent[attribType];
		}

		loaderOffset += loader.size;
	}

	u32 fixedAttributes = fixedAttribMask;
	accel.fixedAttributes = 0;

	// Fetch values for all fixed attributes using CLZ on the fixed attribute mask to find the attributes that are actually fixed
	while (fixedAttributes != 0) {
		// Get index of next fixed attribute and turn it off
		const u32 index = std::countr_zero<u32>(fixedAttributes);
		const u32 mask = 1u << index;
		fixedAttributes ^= mask;

		// PICA register this fixed attribute is meant to go to
		const u32 inputReg = (inputAttrCfg >> (index * 4)) & 0xf;
		const u32 inputRegMask = 1u << inputReg;

		// If this input reg is already used for a non-fixed attribute then it will not be replaced by a fixed attribute
		if ((accel.enabledAttributeMask & inputRegMask) == 0) {
			vec4f& fixedAttr = shaderUnit.vs.fixedAttributes[index];
			auto& attr = accel.attributeInfo[inputReg];

			accel.fixedAttributes |= inputRegMask;

			for (int i = 0; i < 4; i++) {
				attr.fixedValue[i] = fixedAttr[i].toFloat32();
			}
		}
	}

	accel.canBeAccelerated = true;
}