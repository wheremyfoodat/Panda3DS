#include "PICA/shader.hpp"

void PICAShader::run() {
	u32 pc = 0; // Program counter

	while (true) {
		const u32 instruction = loadedShader[pc++];
		const u32 opcode = instruction >> 26; // Top 6 bits are the opcode

		switch (opcode) {
			case ShaderOpcodes::MOV: mov(instruction); break;
			default:Helpers::panic("Unimplemented PICA instruction %08X (Opcode = %02X)", instruction, opcode);
		}
	}
}

PICAShader::vec4f PICAShader::getSource(u32 source) {
	if (source < 16)
		return attributes[source];
	else if (source >= 0x20 && source <= 0x7f)
		return floatUniforms[source - 0x20];

	Helpers::panic("[PICA] Unimplemented source value: %X", source);
}

PICAShader::vec4f& PICAShader::getDest(u32 dest) {
	if (dest >= 0x10 && dest <= 0x1f) { // Temporary registers
		return tempRegisters[dest - 0x10];
	}
	Helpers::panic("[PICA] Unimplemented dest: %X", dest);
}

void PICAShader::mov(u32 instruction) {
	const u32 operandDescriptor = operandDescriptors[instruction & 0x7f];
	const u32 src = (instruction >> 12) & 0x7f;
	const u32 idx = (instruction >> 19) & 3;
	const u32 dest = (instruction >> 21) & 0x1f;

	if (idx) Helpers::panic("[PICA] MOV: idx != 0");
	vec4f srcVector = getSource(src);
	srcVector = swizzle<1>(srcVector, operandDescriptor);
	vec4f& destVector = getDest(dest);

	// Destination component mask. Tells us which lanes of the destination register will be written to
	u32 componentMask = operandDescriptor & 0xf;
	for (int i = 0; i < 4; i++) {
		if (componentMask & (1 << i)) {
			destVector[3 - i] = srcVector[3 - i];
		}
	}
}