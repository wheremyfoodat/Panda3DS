#include "PICA/shader.hpp"

void PICAShader::run() {
	pc = 0;
	loopIndex = 0;
	ifIndex = 0;

	while (true) {
		const u32 instruction = loadedShader[pc++];
		const u32 opcode = instruction >> 26; // Top 6 bits are the opcode

		switch (opcode) {
			case ShaderOpcodes::ADD: add(instruction); break;
			case ShaderOpcodes::CMP1: case ShaderOpcodes::CMP2: 
				cmp(instruction);
				break;
			case ShaderOpcodes::DP3: dp3(instruction); break;
			case ShaderOpcodes::DP4: dp4(instruction); break;
			case ShaderOpcodes::END: return; // Stop running shader
			case ShaderOpcodes::IFC: ifc(instruction); break;
			case ShaderOpcodes::LOOP: loop(instruction); break;
			case ShaderOpcodes::MIN: min(instruction); break;
			case ShaderOpcodes::MOV: mov(instruction); break;
			case ShaderOpcodes::MOVA: mova(instruction); break;
			case ShaderOpcodes::MUL: mul(instruction); break;
			case ShaderOpcodes::NOP: break; // Do nothing
			default:Helpers::panic("Unimplemented PICA instruction %08X (Opcode = %02X)", instruction, opcode);
		}

		// Handle control flow statements. The ordering is important as the priority goes: LOOP > IF > CALL
		// Handle loop
		if (loopIndex != 0) {
			auto& loop = loopInfo[loopIndex - 1];
			if (pc == loop.endingPC) { // Check if the loop needs to start over
				loop.iterations -= 1;
				if (loop.iterations == 0) // If the loop ended, go one level down on the loop stack
					loopIndex -= 1;

				loopCounter += loop.increment;
				pc = loop.startingPC;
			}
		}

		// Handle ifs
		if (ifIndex != 0) {
			auto& info = conditionalInfo[ifIndex - 1];
			if (pc == info.endingPC) { // Check if the IF block ended
				pc = info.newPC;
				ifIndex -= 1;
			}
		}
	}
}

// Calculate the actual source value using an instruction's source field and it's respective index value
// The index value is used to apply relative addressing when index != 0 by adding one of the 3 addr registers to the
// source field, but only with the original source field is pointing at a vector uniform register 
u8 PICAShader::getIndexedSource(u32 source, u32 index) {
	if (source < 0x20) // No offset is applied if the source isn't pointing to a vector uniform reg
		return source;

	switch (index) {
		case 0: [[likely]] return u8(source); // No offset applied
		case 1: return u8(source + addrRegister.x());
		case 2: return u8(source + addrRegister.y());
		case 3: return u8(source + loopCounter);
	}

	Helpers::panic("Reached unreachable path in PICAShader::getIndexedSource");
	return 0;
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

bool PICAShader::isCondTrue(u32 instruction) {
	u32 condition = (instruction >> 22) & 3;
	bool refX = ((instruction >> 24) & 1) != 0;
	bool refY = ((instruction >> 25) & 1) != 0;

	switch (condition) {
		case 0: // Either cmp register matches 
			return cmpRegister[0] == refX || cmpRegister[1] == refY;
		case 1: // Both cmp registers match
			return cmpRegister[0] == refX && cmpRegister[1] == refY;
		case 2: // At least cmp.x matches
			return cmpRegister[0] == refX;
		case 3: // At least cmp.y matches
			return cmpRegister[1] == refY;
	}
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

void PICAShader::mul(u32 instruction) {
	const u32 operandDescriptor = operandDescriptors[instruction & 0x7f];
	const u32 src1 = (instruction >> 12) & 0x7f;
	const u32 src2 = (instruction >> 7) & 0x1f; // src2 coming first because PICA moment
	const u32 idx = (instruction >> 19) & 3;
	const u32 dest = (instruction >> 21) & 0x1f;

	if (idx) Helpers::panic("[PICA] MUL: idx != 0");
	vec4f srcVec1 = getSourceSwizzled<1>(src1, operandDescriptor);
	vec4f srcVec2 = getSourceSwizzled<2>(src2, operandDescriptor);

	vec4f& destVector = getDest(dest);

	u32 componentMask = operandDescriptor & 0xf;
	for (int i = 0; i < 4; i++) {
		if (componentMask & (1 << i)) {
			destVector[3 - i] = srcVec1[3 - i] * srcVec2[3 - 1];
		}
	}
}

void PICAShader::min(u32 instruction) {
	const u32 operandDescriptor = operandDescriptors[instruction & 0x7f];
	const u32 src1 = (instruction >> 12) & 0x7f;
	const u32 src2 = (instruction >> 7) & 0x1f; // src2 coming first because PICA moment
	const u32 idx = (instruction >> 19) & 3;
	const u32 dest = (instruction >> 21) & 0x1f;

	if (idx) Helpers::panic("[PICA] MIN: idx != 0");
	vec4f srcVec1 = getSourceSwizzled<1>(src1, operandDescriptor);
	vec4f srcVec2 = getSourceSwizzled<2>(src2, operandDescriptor);

	vec4f& destVector = getDest(dest);

	u32 componentMask = operandDescriptor & 0xf;
	for (int i = 0; i < 4; i++) {
		if (componentMask & (1 << i)) {
			const auto mininum = srcVec1[3 - i] < srcVec2[3 - i] ? srcVec1[3 - i] : srcVec2[3 - i];
			destVector[3 - i] = mininum;
		}
	}
}

void PICAShader::mov(u32 instruction) {
	const u32 operandDescriptor = operandDescriptors[instruction & 0x7f];
	u32 src = (instruction >> 12) & 0x7f;
	const u32 idx = (instruction >> 19) & 3;
	const u32 dest = (instruction >> 21) & 0x1f;

	src = getIndexedSource(src, idx);
	vec4f srcVector = getSourceSwizzled<1>(src, operandDescriptor);
	vec4f& destVector = getDest(dest);

	u32 componentMask = operandDescriptor & 0xf;
	for (int i = 0; i < 4; i++) {
		if (componentMask & (1 << i)) {
			destVector[3 - i] = srcVector[3 - i];
		}
	}
}

void PICAShader::mova(u32 instruction) {
	const u32 operandDescriptor = operandDescriptors[instruction & 0x7f];
	const u32 src = (instruction >> 12) & 0x7f;
	const u32 idx = (instruction >> 19) & 3;
	const u32 dest = (instruction >> 21) & 0x1f;

	if (idx) Helpers::panic("[PICA] MOVA: idx != 0");
	vec4f srcVector = getSourceSwizzled<1>(src, operandDescriptor);

	u32 componentMask = operandDescriptor & 0xf;
	if (componentMask & 0b1000) // x component
		addrRegister.x() = static_cast<s32>(srcVector.x().toFloat32());
	if (componentMask & 0b0100) // y component
		addrRegister.y() = static_cast<s32>(srcVector.y().toFloat32());
}

void PICAShader::dp3(u32 instruction) {
	const u32 operandDescriptor = operandDescriptors[instruction & 0x7f];
	const u32 src1 = (instruction >> 12) & 0x7f;
	const u32 src2 = (instruction >> 7) & 0x1f; // src2 coming first because PICA moment
	const u32 idx = (instruction >> 19) & 3;
	const u32 dest = (instruction >> 21) & 0x1f;

	if (idx) Helpers::panic("[PICA] DP3: idx != 0");
	vec4f srcVec1 = getSourceSwizzled<1>(src1, operandDescriptor);
	vec4f srcVec2 = getSourceSwizzled<2>(src2, operandDescriptor);

	vec4f& destVector = getDest(dest);
	f24 dot = srcVec1[0] * srcVec2[0] + srcVec1[1] * srcVec2[1] + srcVec1[2] * srcVec2[2];

	u32 componentMask = operandDescriptor & 0xf;
	for (int i = 0; i < 4; i++) {
		if (componentMask & (1 << i)) {
			destVector[3 - i] = dot;
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

void PICAShader::cmp(u32 instruction) {
	const u32 operandDescriptor = operandDescriptors[instruction & 0x7f];
	const u32 src1 = (instruction >> 12) & 0x7f;
	const u32 src2 = (instruction >> 7) & 0x1f; // src2 coming first because PICA moment
	const u32 idx = (instruction >> 19) & 3;
	const u32 cmpX = (instruction >> 21) & 7;
	const u32 cmpY = (instruction >> 24) & 7;
	const u32 cmpOperations[2] = { cmpX, cmpY };

	if (idx) Helpers::panic("[PICA] CMP: idx != 0");
	vec4f srcVec1 = getSourceSwizzled<1>(src1, operandDescriptor);
	vec4f srcVec2 = getSourceSwizzled<2>(src2, operandDescriptor);

	for (int i = 0; i < 2; i++) {
		switch (cmpOperations[i]) {
			case 0: // Equal
				cmpRegister[i] = srcVec1[i] == srcVec2[i];
				break;

			case 1: // Not equal
				cmpRegister[i] = srcVec1[i] != srcVec2[i];
				break;

			case 2: // Less than
				cmpRegister[i] = srcVec1[i] < srcVec2[i];
				break;

			case 3: // Less than or equal
				cmpRegister[i] = srcVec1[i] <= srcVec2[i];
				break;

			case 4: // Greater than
				cmpRegister[i] = srcVec1[i] > srcVec2[i];
				break;

			case 5: // Greater than or equal
				cmpRegister[i] = srcVec1[i] >= srcVec2[i];
				break;

			default:
				cmpRegister[i] = true;
				break;
		}
	}
}

void PICAShader::ifc(u32 instruction) {
	const u32 dest = (instruction >> 10) & 0xfff;

	if (isCondTrue(instruction)) {
		if (ifIndex >= 8) [[unlikely]]
			Helpers::panic("[PICA] Overflowed IF stack");

		const u32 num = instruction & 0xff;

		auto& block = conditionalInfo[ifIndex++];
		block.endingPC = dest;
		block.newPC = dest + num;
	} else {
		pc = dest;
	}
}

void PICAShader::loop(u32 instruction) {
	if (loopIndex >= 4) [[unlikely]]
		Helpers::panic("[PICA] Overflowed loop stack");

	u32 dest = (instruction >> 10) & 0xfff;
	auto& uniform = intUniforms[(instruction >> 22) & 3]; // The uniform we'll get loop info from
	loopCounter = uniform.y();
	auto& loop = loopInfo[loopIndex++];

	loop.startingPC = pc;
	loop.endingPC = dest + 1; // Loop is inclusive so we need + 1 here
	loop.iterations = uniform.x() + 1;
	loop.increment = uniform.z();
}