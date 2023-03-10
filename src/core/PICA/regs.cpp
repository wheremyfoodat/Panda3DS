#include "PICA/gpu.hpp"
#include "PICA/regs.hpp"

using namespace Floats;

u32 GPU::readReg(u32 address) {
	if (address >= 0x1EF01000 && address < 0x1EF01C00) { // Internal registers
		const u32 index = (address - 0x1EF01000) / sizeof(u32);
		return readInternalReg(index);
	} else {
		log("Ignoring read to external GPU register %08X.\n", address);
		return 0;
	}
}

void GPU::writeReg(u32 address, u32 value) {
	if (address >= 0x1EF01000 && address < 0x1EF01C00) { // Internal registers
		const u32 index = (address - 0x1EF01000) / sizeof(u32);
		writeInternalReg(index, value, 0xffffffff);
	} else {
		log("Ignoring write to external GPU register %08X. Value: %08X\n", address, value);
	}
}

u32 GPU::readInternalReg(u32 index) {
	if (index > regNum) {
		Helpers::panic("Tried to read invalid GPU register. Index: %X\n", index);
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

		case ColourBufferLoc: {
			u32 loc = (value & 0x0fffffff) << 3;
			renderer.setColourBufferLoc(loc);
			break;
		};

		case ColourBufferFormat: {
			u32 format = (value >> 16) & 7;
			renderer.setColourFormat(format);
			break;
		}

		case DepthBufferLoc: {
			u32 loc = (value & 0x0fffffff) << 3;
			renderer.setDepthBufferLoc(loc);
			break;
		}

		case DepthBufferFormat: {
			u32 fmt = value & 0x3;
			renderer.setDepthFormat(fmt);
			break;
		}

		case FramebufferSize: {
			const u32 width = value & 0x7ff;
			const u32 height = ((value >> 12) & 0x3ff) + 1;
			renderer.setFBSize(width, height);
			break;
		}

		case VertexFloatUniformIndex:
			shaderUnit.vs.setFloatUniformIndex(value);
			break;

		case VertexFloatUniformData0: case VertexFloatUniformData1: case VertexFloatUniformData2:
		case VertexFloatUniformData3: case VertexFloatUniformData4: case VertexFloatUniformData5:
		case VertexFloatUniformData6: case VertexFloatUniformData7:
			shaderUnit.vs.uploadFloatUniform(value);
			break;

		case FixedAttribIndex:
			fixedAttribCount = 0;
			fixedAttribIndex = value & 0xf;

			if (fixedAttribIndex == 0xf) {
				log("[PICA] Immediate mode vertex submission enabled");
				immediateModeAttrIndex = 0;
				immediateModeVertIndex = 0;
			}
			break;

		case FixedAttribData0: case FixedAttribData1: case FixedAttribData2:
			fixedAttrBuff[fixedAttribCount++] = value;

			if (fixedAttribCount == 3) {
				fixedAttribCount = 0;

				vec4f attr;
				// These are stored in the reverse order anyone would expect them to be in
				attr.x() = f24::fromRaw(fixedAttrBuff[2] & 0xffffff);
				attr.y() = f24::fromRaw(((fixedAttrBuff[1] & 0xffff) << 8) | (fixedAttrBuff[2] >> 24));
				attr.z() = f24::fromRaw(((fixedAttrBuff[0] & 0xff) << 16) | (fixedAttrBuff[1] >> 16));
				attr.w() = f24::fromRaw(fixedAttrBuff[0] >> 8);

				// If the fixed attribute index is < 12, we're just writing to one of the fixed attributes
				if (fixedAttribIndex < 12) [[likely]] {
					shaderUnit.vs.fixedAttributes[fixedAttribIndex++] = attr;
				} else if (fixedAttribIndex == 15) { // Otherwise if it's 15, we're submitting an immediate mode vertex
					const uint totalAttrCount = (regs[PICAInternalRegs::VertexShaderAttrNum] & 0xf) + 1;
					if (totalAttrCount <= immediateModeAttrIndex) {
						printf("Broken state in the immediate mode vertex submission pipeline. Failing silently\n");
						immediateModeAttrIndex = 0;
						immediateModeVertIndex = 0;
					}

					immediateModeAttributes[immediateModeAttrIndex++] = attr;
					if (immediateModeAttrIndex == totalAttrCount) {
						Vertex v = getImmediateModeVertex();
						immediateModeAttrIndex = 0;
						immediateModeVertices[immediateModeVertIndex++] = v;

						// If we've reached 3 verts, issue a draw call
						if (immediateModeVertIndex == 3) {
							renderer.drawVertices(OpenGL::Triangle, &immediateModeVertices[0], 3);
							immediateModeVertIndex = 0;
						}
					}
				} else { // Writing to fixed attributes 13 and 14 probably does nothing, but we'll see
					log("Wrote to invalid fixed vertex attribute %d\n", fixedAttribIndex);
				}
			}

			break;

		case VertexShaderOpDescriptorIndex:
			shaderUnit.vs.setOpDescriptorIndex(value);
			break;

		case VertexShaderOpDescriptorData0: case VertexShaderOpDescriptorData1: case VertexShaderOpDescriptorData2:
		case VertexShaderOpDescriptorData3: case VertexShaderOpDescriptorData4: case VertexShaderOpDescriptorData5:
		case VertexShaderOpDescriptorData6: case VertexShaderOpDescriptorData7:
			shaderUnit.vs.uploadDescriptor(value);
			break;

		case VertexBoolUniform:
			shaderUnit.vs.boolUniform = value & 0xffff;
			break;

		case VertexIntUniform0: case VertexIntUniform1: case VertexIntUniform2: case VertexIntUniform3:
			shaderUnit.vs.uploadIntUniform(index - VertexIntUniform0, value);
			break;

		case VertexShaderData0: case VertexShaderData1: case VertexShaderData2: case VertexShaderData3:
		case VertexShaderData4: case VertexShaderData5: case VertexShaderData6: case VertexShaderData7:
			shaderUnit.vs.uploadWord(value);
			break;

		case VertexShaderEntrypoint:
			shaderUnit.vs.entrypoint = value & 0xffff;
			break;

		case VertexShaderTransferEnd:
			if (value != 0) shaderUnit.vs.finalize();
			break;

		case VertexShaderTransferIndex:
			shaderUnit.vs.setBufferIndex(value);
			break;

		// Command lists can write to the command processor registers and change the command list stream
		// Several games are known to do this, including New Super Mario Bros 2 and Super Mario 3D Land
		case CmdBufTrigger0:
		case CmdBufTrigger1: {
			if (value != 0) { // A non-zero value triggers command list processing
				int bufferIndex = index - CmdBufTrigger0; // Index of the command buffer to execute (0 or 1)
				u32 addr = (regs[CmdBufAddr0 + bufferIndex] & 0xfffffff) << 3;
				u32 size = (regs[CmdBufSize0 + bufferIndex] & 0xfffff) << 3;

				// Set command buffer state to execute the new buffer
				cmdBuffStart = getPointerPhys<u32>(addr);
				cmdBuffCurr = cmdBuffStart;
				cmdBuffEnd = cmdBuffStart + (size / sizeof(u32));
			}
			break;
		}

		default:
			// Vertex attribute registers
			if (index >= AttribInfoStart && index <= AttribInfoEnd) {
				uint attributeIndex = (index - AttribInfoStart) / 3; // Which attribute are we writing to
				uint reg = (index - AttribInfoStart) % 3; // Which of this attribute's registers are we writing to?
				auto& attr = attributeInfo[attributeIndex];

				switch (reg) {
					case 0: attr.offset = value & 0xfffffff; break; // Attribute offset
					case 1: 
						attr.config1 = value;
						break;
					case 2:
						attr.config2 = value;
						attr.size = (value >> 16) & 0xff;
						attr.componentCount = value >> 28;
						break;
				}
			} else {
				log("GPU: Wrote to unimplemented internal reg: %X, value: %08X\n", index, newValue);
			}
			break;
	}
}

void GPU::startCommandList(u32 addr, u32 size) {
	cmdBuffStart = static_cast<u32*>(mem.getReadPointer(addr));
	if (!cmdBuffStart) Helpers::panic("Couldn't get buffer for command list");
	// TODO: This is very memory unsafe. We get a pointer to FCRAM and just keep writing without checking if we're gonna go OoB

	cmdBuffCurr = cmdBuffStart;
	cmdBuffEnd = cmdBuffStart + (size / sizeof(u32));

	// LUT for converting the parameter mask to an actual 32-bit mask
	// The parameter mask is 4 bits long, each bit corresponding to one byte of the mask
	// If the bit is 0 then the corresponding mask byte is 0, otherwise the mask byte is 0xff
	// So for example if the parameter mask is 0b1001, the full mask is 0xff'00'00'ff
	static constexpr std::array<u32, 16> maskLUT = {
		0x00000000, 0x000000ff, 0x0000ff00, 0x0000ffff, 0x00ff0000, 0x00ff00ff, 0x00ffff00, 0x00ffffff,
		0xff000000, 0xff0000ff, 0xff00ff00, 0xff00ffff, 0xffff0000, 0xffff00ff, 0xffffff00, 0xffffffff,
	};

	while (cmdBuffCurr < cmdBuffEnd) {
		// If the buffer is not aligned to an 8 byte boundary, force align it by moving the pointer up a word
		// The curr pointer starts out doubleword-aligned and is increased by 4 bytes each time
		// So to check if it is aligned, we get the number of words it's been incremented by
		// If that number is an odd value then the buffer is not aligned, otherwise it is
		if ((cmdBuffCurr - cmdBuffStart) % 2 != 0) {
			cmdBuffCurr++;
		}

		// The first word of a command is the command parameter and the second one is the header
		u32 param1 = *cmdBuffCurr++;
		u32 header = *cmdBuffCurr++;

		u32 id = header & 0xffff;
		u32 paramMaskIndex = (header >> 16) & 0xf;
		u32 paramCount = (header >> 20) & 0xff; // Number of additional parameters
		// Bit 31 tells us whether this command is going to write to multiple sequential registers (if the bit is 1)
		// Or if all written values will go to the same register (If the bit is 0). It's essentially the value that
		// gets added to the "id" field after each register write
		bool consecutiveWritingMode = (header >> 31) != 0;

		u32 mask = maskLUT[paramMaskIndex]; // Actual parameter mask
		// Increment the ID by 1 after each write if we're in consecutive mode, or 0 otherwise
		u32 idIncrement = (consecutiveWritingMode) ? 1 : 0;

		writeInternalReg(id, param1, mask);
		for (u32 i = 0; i < paramCount; i++) {
			id += idIncrement;
			u32 param = *cmdBuffCurr++;
			writeInternalReg(id, param, mask);
		}
	}
}