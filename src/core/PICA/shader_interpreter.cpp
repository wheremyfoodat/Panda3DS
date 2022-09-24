#include "PICA/shader.hpp"

void PICAShader::run() {
	u32 pc = 0; // Program counter

	while (true) {
		const u32 instruction = loadedShader[pc++];
		const u32 opcode = instruction >> 26; // Top 6 bits are the opcode

		switch (opcode) {
			case ShaderOpcodes::ADD: add(instruction); break;
			case ShaderOpcodes::DP4: dp4(instruction); break;
			case ShaderOpcodes::END: return; // Stop running shader
			case ShaderOpcodes::MOV: mov(instruction); break;
			default:Helpers::panic("Unimplemented PICA instruction %08X (Opcode = %02X)", instruction, opcode);
		}
	}
}

PICAShader::vec4f PICAShader::getSource(u32 source) {
	if (source < 0x10)
		return attributes[source];
	else if (source < 0x20)
		return tempRegisters[source - 0x10];
	else if (source <= 0x7f)
		return floatUniforms[source - 0x20];

	Helpers::panic("[PICA] Unimplemented source value: %X", source);
}

PICAShader::vec4f& PICAShader::getDest(u32 dest) {
	if (dest <= 0x6) {
		return outputs[dest];
	} else if (dest >= 0x10 && dest <= 0x1f) { // Temporary registers
		return tempRegisters[dest - 0x10];
	}
	Helpers::panic("[PICA] Unimplemented dest: %X", dest);
}

void PICAShader::add(u32 instruction) {
	const u32 operandDescriptor = operandDescriptors[instruction & 0x7f];
	const u32 src1 = (instruction >> 12) & 0x7f;
	const u32 src2 = (instruction >> 7) & 0x1f; // src2 coming first because PICA moment
	const u32 idx = (instruction >> 19) & 3;
	const u32 dest = (instruction >> 21) & 0x1f;

	if (idx) Helpers::panic("[PICA] ADD: idx != 0");
	vec4f srcVec1 = getSourceSwizzled<1>(src1, operandDescriptor);
	vec4f srcVec2 = getSourceSwizzled<2>(src2, operandDescriptor);

	vec4f& destVector = getDest(dest);

	u32 componentMask = operandDescriptor & 0xf;
	for (int i = 0; i < 4; i++) {
		if (componentMask & (1 << i)) {
			destVector[3 - i] = srcVec1[3 - i] + srcVec2[3 - 1];
		}
	}
}

void PICAShader::mov(u32 instruction) {
	const u32 operandDescriptor = operandDescriptors[instruction & 0x7f];
	const u32 src = (instruction >> 12) & 0x7f;
	const u32 idx = (instruction >> 19) & 3;
	const u32 dest = (instruction >> 21) & 0x1f;

	if (idx) Helpers::panic("[PICA] MOV: idx != 0");
	vec4f srcVector = getSourceSwizzled<1>(src, operandDescriptor);
	vec4f& destVector = getDest(dest);

	u32 componentMask = operandDescriptor & 0xf;
	for (int i = 0; i < 4; i++) {
		if (componentMask & (1 << i)) {
			destVector[3 - i] = srcVector[3 - i];
		}
	}
}

void PICAShader::dp4(u32 instruction) {
	const u32 operandDescriptor = operandDescriptors[instruction & 0x7f];
	const u32 src1 = (instruction >> 12) & 0x7f;
	const u32 src2 = (instruction >> 7) & 0x1f; // src2 coming first because PICA moment
	const u32 idx = (instruction >> 19) & 3;
	const u32 dest = (instruction >> 21) & 0x1f;

	if (idx) Helpers::panic("[PICA] DP4: idx != 0");
	vec4f srcVec1 = getSourceSwizzled<1>(src1, operandDescriptor);
	vec4f srcVec2 = getSourceSwizzled<2>(src2, operandDescriptor);

	vec4f& destVector = getDest(dest);
	f24 dot = srcVec1[0] * srcVec2[0] + srcVec1[1] * srcVec2[1] + srcVec1[2] * srcVec2[2] + srcVec1[3] * srcVec2[3];

	u32 componentMask = operandDescriptor & 0xf;
	for (int i = 0; i < 4; i++) {
		if (componentMask & (1 << i)) {
			destVector[3 - i] = dot;
		}
	}
}