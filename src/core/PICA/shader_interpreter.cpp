#include <cmath>

#include "PICA/shader.hpp"

using namespace Helpers;

void PICAShader::run() {
	pc = entrypoint;
	loopIndex = 0;
	ifIndex = 0;
	callIndex = 0;

	while (true) {
		const u32 instruction = loadedShader[pc++];
		const u32 opcode = instruction >> 26;  // Top 6 bits are the opcode

		switch (opcode) {
			case ShaderOpcodes::ADD: add(instruction); break;
			case ShaderOpcodes::CALL: call(instruction); break;
			case ShaderOpcodes::CALLC: callc(instruction); break;
			case ShaderOpcodes::CALLU: callu(instruction); break;
			case ShaderOpcodes::CMP1:
			case ShaderOpcodes::CMP2: {
				cmp(instruction);
				break;
			}

			case ShaderOpcodes::DP3: dp3(instruction); break;
			case ShaderOpcodes::DP4: dp4(instruction); break;
			case ShaderOpcodes::DPHI: dphi(instruction); break;
			case ShaderOpcodes::END: return;  // Stop running shader
			case ShaderOpcodes::EX2: ex2(instruction); break;
			case ShaderOpcodes::FLR: flr(instruction); break;
			case ShaderOpcodes::IFC: ifc(instruction); break;
			case ShaderOpcodes::IFU: ifu(instruction); break;
			case ShaderOpcodes::JMPC: jmpc(instruction); break;
			case ShaderOpcodes::JMPU: jmpu(instruction); break;
			case ShaderOpcodes::LG2: lg2(instruction); break;
			case ShaderOpcodes::LOOP: loop(instruction); break;
			case ShaderOpcodes::MAX: max(instruction); break;
			case ShaderOpcodes::MIN: min(instruction); break;
			case ShaderOpcodes::MOV: mov(instruction); break;
			case ShaderOpcodes::MOVA: mova(instruction); break;
			case ShaderOpcodes::MUL: mul(instruction); break;
			case ShaderOpcodes::NOP: break;  // Do nothing
			case ShaderOpcodes::RCP: rcp(instruction); break;
			case ShaderOpcodes::RSQ: rsq(instruction); break;
			case ShaderOpcodes::SGE: sge(instruction); break;
			case ShaderOpcodes::SGEI: sgei(instruction); break;
			case ShaderOpcodes::SLT: slt(instruction); break;
			case ShaderOpcodes::SLTI: slti(instruction); break;

			case 0x30:
			case 0x31:
			case 0x32:
			case 0x33:
			case 0x34:
			case 0x35:
			case 0x36:
			case 0x37: {
				madi(instruction);
				break;
			}

			case 0x38:
			case 0x39:
			case 0x3A:
			case 0x3B:
			case 0x3C:
			case 0x3D:
			case 0x3E:
			case 0x3F: {
				mad(instruction);
				break;
			}

			default: Helpers::panic("Unimplemented PICA instruction %08X (Opcode = %02X)", instruction, opcode);
		}

		// Handle control flow statements. The ordering is important as the priority goes: LOOP > IF > CALL
		// Handle loop
		if (loopIndex != 0) {
			auto& loop = loopInfo[loopIndex - 1];
			if (pc == loop.endingPC) {  // Check if the loop needs to start over
				loop.iterations -= 1;
				if (loop.iterations == 0)  // If the loop ended, go one level down on the loop stack
					loopIndex -= 1;

				loopCounter += loop.increment;
				pc = loop.startingPC;
			}
		}

		// Handle ifs
		if (ifIndex != 0) {
			auto& info = conditionalInfo[ifIndex - 1];
			if (pc == info.endingPC) {  // Check if the IF block ended
				pc = info.newPC;
				ifIndex -= 1;
			}
		}

		// Handle calls
		if (callIndex != 0) {
			auto& info = callInfo[callIndex - 1];
			if (pc == info.endingPC) {  // Check if the CALL block ended
				pc = info.returnPC;
				callIndex -= 1;
			}
		}
	}
}

// Calculate the actual source value using an instruction's source field and it's respective index value
// The index value is used to apply relative addressing when index != 0 by adding one of the 3 addr registers to the
// source field, but only with the original source field is pointing at a vector uniform register
u8 PICAShader::getIndexedSource(u32 source, u32 index) {
	if (source < 0x20)  // No offset is applied if the source isn't pointing to a vector uniform reg
		return source;

	switch (index) {
		case 0: [[likely]] return u8(source);  // No offset applied
		case 1: return u8(source + addrRegister[0]);
		case 2: return u8(source + addrRegister[1]);
		case 3: return u8(source + loopCounter);
	}

	Helpers::panic("Reached unreachable path in PICAShader::getIndexedSource");
	return 0;
}

PICAShader::vec4f PICAShader::getSource(u32 source) {
	if (source < 0x10)
		return inputs[source];
	else if (source < 0x20)
		return tempRegisters[source - 0x10];
	else if (source <= 0x7f)
		return floatUniforms[source - 0x20];
	else {
		Helpers::warn("[PICA] Unimplemented source value: %X\n", source);
		return vec4f({f24::zero(), f24::zero(), f24::zero(), f24::zero()});
	}
}

PICAShader::vec4f& PICAShader::getDest(u32 dest) {
	if (dest < 0x10) {
		return outputs[dest];
	} else if (dest < 0x20) {
		return tempRegisters[dest - 0x10];
	}
	Helpers::panic("[PICA] Unimplemented dest: %X", dest);
}

bool PICAShader::isCondTrue(u32 instruction) {
	u32 condition = getBits<22, 2>(instruction);
	bool refY = (getBit<24>(instruction)) != 0;
	bool refX = (getBit<25>(instruction)) != 0;

	switch (condition) {
		case 0:  // Either cmp register matches
			return cmpRegister[0] == refX || cmpRegister[1] == refY;
		case 1:  // Both cmp registers match
			return cmpRegister[0] == refX && cmpRegister[1] == refY;
		case 2:  // At least cmp.x matches
			return cmpRegister[0] == refX;
		default:  // At least cmp.y matches
			return cmpRegister[1] == refY;
	}
}

void PICAShader::add(u32 instruction) {
	const u32 operandDescriptor = operandDescriptors[instruction & 0x7f];
	u32 src1 = getBits<12, 7>(instruction);
	const u32 src2 = getBits<7, 5>(instruction);  // src2 coming first because PICA moment
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	src1 = getIndexedSource(src1, idx);
	vec4f srcVec1 = getSourceSwizzled<1>(src1, operandDescriptor);
	vec4f srcVec2 = getSourceSwizzled<2>(src2, operandDescriptor);

	vec4f& destVector = getDest(dest);

	u32 componentMask = operandDescriptor & 0xf;
	for (int i = 0; i < 4; i++) {
		if (componentMask & (1 << i)) {
			destVector[3 - i] = srcVec1[3 - i] + srcVec2[3 - i];
		}
	}
}

void PICAShader::mul(u32 instruction) {
	const u32 operandDescriptor = operandDescriptors[instruction & 0x7f];
	u32 src1 = getBits<12, 7>(instruction);
	const u32 src2 = getBits<7, 5>(instruction);  // src2 coming first because PICA moment
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	src1 = getIndexedSource(src1, idx);
	vec4f srcVec1 = getSourceSwizzled<1>(src1, operandDescriptor);
	vec4f srcVec2 = getSourceSwizzled<2>(src2, operandDescriptor);

	vec4f& destVector = getDest(dest);

	u32 componentMask = operandDescriptor & 0xf;
	for (int i = 0; i < 4; i++) {
		if (componentMask & (1 << i)) {
			destVector[3 - i] = srcVec1[3 - i] * srcVec2[3 - i];
		}
	}
}

void PICAShader::flr(u32 instruction) {
	const u32 operandDescriptor = operandDescriptors[instruction & 0x7f];
	u32 src = getBits<12, 7>(instruction);
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	src = getIndexedSource(src, idx);
	vec4f srcVector = getSourceSwizzled<1>(src, operandDescriptor);
	vec4f& destVector = getDest(dest);

	u32 componentMask = operandDescriptor & 0xf;
	for (int i = 0; i < 4; i++) {
		if (componentMask & (1 << i)) {
			destVector[3 - i] = f24::fromFloat32(std::floor(srcVector[3 - i].toFloat32()));
		}
	}
}

void PICAShader::max(u32 instruction) {
	const u32 operandDescriptor = operandDescriptors[instruction & 0x7f];
	const u32 src1 = getBits<12, 7>(instruction);
	const u32 src2 = getBits<7, 5>(instruction);  // src2 coming first because PICA moment
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	if (idx) Helpers::panic("[PICA] MAX: idx != 0");
	vec4f srcVec1 = getSourceSwizzled<1>(src1, operandDescriptor);
	vec4f srcVec2 = getSourceSwizzled<2>(src2, operandDescriptor);

	vec4f& destVector = getDest(dest);

	u32 componentMask = operandDescriptor & 0xf;
	for (int i = 0; i < 4; i++) {
		if (componentMask & (1 << i)) {
			const float inputA = srcVec1[3 - i].toFloat32();
			const float inputB = srcVec2[3 - i].toFloat32();
			// max(NaN, 2.f) -> NaN
			// max(2.f, NaN) -> 2
			const auto& maximum = std::isinf(inputB) ? inputB : std::max(inputB, inputA);
			destVector[3 - i] = f24::fromFloat32(maximum);
		}
	}
}

void PICAShader::min(u32 instruction) {
	const u32 operandDescriptor = operandDescriptors[instruction & 0x7f];
	const u32 src1 = getBits<12, 7>(instruction);
	const u32 src2 = getBits<7, 5>(instruction);  // src2 coming first because PICA moment
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	if (idx) Helpers::panic("[PICA] MIN: idx != 0");
	vec4f srcVec1 = getSourceSwizzled<1>(src1, operandDescriptor);
	vec4f srcVec2 = getSourceSwizzled<2>(src2, operandDescriptor);

	vec4f& destVector = getDest(dest);

	u32 componentMask = operandDescriptor & 0xf;
	for (int i = 0; i < 4; i++) {
		if (componentMask & (1 << i)) {
			const float inputA = srcVec1[3 - i].toFloat32();
			const float inputB = srcVec2[3 - i].toFloat32();
			// min(NaN, 2.f) -> NaN
			// min(2.f, NaN) -> 2
			const auto& mininum = std::min(inputB, inputA);
			destVector[3 - i] = f24::fromFloat32(mininum);
		}
	}
}

void PICAShader::mov(u32 instruction) {
	const u32 operandDescriptor = operandDescriptors[instruction & 0x7f];
	u32 src = getBits<12, 7>(instruction);
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

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
	const u32 src = getBits<12, 7>(instruction);
	const u32 idx = getBits<19, 2>(instruction);

	if (idx) Helpers::panic("[PICA] MOVA: idx != 0");
	vec4f srcVector = getSourceSwizzled<1>(src, operandDescriptor);

	u32 componentMask = operandDescriptor & 0xf;
	if (componentMask & 0b1000)  // x component
		addrRegister[0] = static_cast<s32>(srcVector[0].toFloat32());
	if (componentMask & 0b0100)  // y component
		addrRegister[1] = static_cast<s32>(srcVector[1].toFloat32());
}

void PICAShader::dp3(u32 instruction) {
	const u32 operandDescriptor = operandDescriptors[instruction & 0x7f];
	u32 src1 = getBits<12, 7>(instruction);
	const u32 src2 = getBits<7, 5>(instruction);  // src2 coming first because PICA moment
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	src1 = getIndexedSource(src1, idx);
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
	u32 src1 = getBits<12, 7>(instruction);
	const u32 src2 = getBits<7, 5>(instruction);  // src2 coming first because PICA moment
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	src1 = getIndexedSource(src1, idx);
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

void PICAShader::dphi(u32 instruction) {
	const u32 operandDescriptor = operandDescriptors[instruction & 0x7f];
	const u32 src1 = getBits<14, 5>(instruction);
	u32 src2 = getBits<7, 7>(instruction);
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	src2 = getIndexedSource(src2, idx);
	vec4f srcVec1 = getSourceSwizzled<1>(src1, operandDescriptor);
	vec4f srcVec2 = getSourceSwizzled<2>(src2, operandDescriptor);

	vec4f& destVector = getDest(dest);
	// srcVec1[3] is supposed to be replaced with 1.0 in the dot product, so we just add srcVec2[3] without multiplying it with anything
	f24 dot = srcVec1[0] * srcVec2[0] + srcVec1[1] * srcVec2[1] + srcVec1[2] * srcVec2[2] + srcVec2[3];

	u32 componentMask = operandDescriptor & 0xf;
	for (int i = 0; i < 4; i++) {
		if (componentMask & (1 << i)) {
			destVector[3 - i] = dot;
		}
	}
}

void PICAShader::rcp(u32 instruction) {
	const u32 operandDescriptor = operandDescriptors[instruction & 0x7f];
	const u32 src1 = getBits<12, 7>(instruction);
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	if (idx) Helpers::panic("[PICA] RCP: idx != 0");
	vec4f srcVec1 = getSourceSwizzled<1>(src1, operandDescriptor);

	vec4f& destVector = getDest(dest);
	float input = srcVec1[0].toFloat32();
	if (input == -0.0f) {
		input = 0.0f;
	}
	const f24 res = f24::fromFloat32(1.0f / input);

	u32 componentMask = operandDescriptor & 0xf;
	for (int i = 0; i < 4; i++) {
		if (componentMask & (1 << i)) {
			destVector[3 - i] = res;
		}
	}
}

void PICAShader::rsq(u32 instruction) {
	const u32 operandDescriptor = operandDescriptors[instruction & 0x7f];
	const u32 src1 = getBits<12, 7>(instruction);
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	if (idx) Helpers::panic("[PICA] RSQ: idx != 0");
	vec4f srcVec1 = getSourceSwizzled<1>(src1, operandDescriptor);

	vec4f& destVector = getDest(dest);
	float input = srcVec1[0].toFloat32();
	if (input == -0.0f) {
		input = 0.0f;
	}
	const f24 res = f24::fromFloat32(1.0f / std::sqrt(input));

	u32 componentMask = operandDescriptor & 0xf;
	for (int i = 0; i < 4; i++) {
		if (componentMask & (1 << i)) {
			destVector[3 - i] = res;
		}
	}
}

void PICAShader::ex2(u32 instruction) {
	const u32 operandDescriptor = operandDescriptors[instruction & 0x7f];
	u32 src = getBits<12, 7>(instruction);
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	src = getIndexedSource(src, idx);
	vec4f srcVec = getSourceSwizzled<1>(src, operandDescriptor);

	vec4f& destVector = getDest(dest);
	f24 res = f24::fromFloat32(std::exp2(srcVec[0].toFloat32()));

	u32 componentMask = operandDescriptor & 0xf;
	for (int i = 0; i < 4; i++) {
		if (componentMask & (1 << i)) {
			destVector[3 - i] = res;
		}
	}
}

void PICAShader::lg2(u32 instruction) {
	const u32 operandDescriptor = operandDescriptors[instruction & 0x7f];
	u32 src = getBits<12, 7>(instruction);
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	src = getIndexedSource(src, idx);
	vec4f srcVec = getSourceSwizzled<1>(src, operandDescriptor);

	vec4f& destVector = getDest(dest);
	f24 res = f24::fromFloat32(std::log2(srcVec[0].toFloat32()));

	u32 componentMask = operandDescriptor & 0xf;
	for (int i = 0; i < 4; i++) {
		if (componentMask & (1 << i)) {
			destVector[3 - i] = res;
		}
	}
}

void PICAShader::mad(u32 instruction) {
	const u32 operandDescriptor = operandDescriptors[instruction & 0x1f];
	const u32 src1 = getBits<17, 5>(instruction);
	u32 src2 = getBits<10, 7>(instruction);
	const u32 src3 = getBits<5, 5>(instruction);
	const u32 idx = getBits<22, 2>(instruction);
	const u32 dest = getBits<24, 5>(instruction);

	src2 = getIndexedSource(src2, idx);

	auto srcVec1 = getSourceSwizzled<1>(src1, operandDescriptor);
	auto srcVec2 = getSourceSwizzled<2>(src2, operandDescriptor);
	auto srcVec3 = getSourceSwizzled<3>(src3, operandDescriptor);
	auto& destVector = getDest(dest);

	u32 componentMask = operandDescriptor & 0xf;
	for (int i = 0; i < 4; i++) {
		if (componentMask & (1 << i)) {
			destVector[3 - i] = srcVec1[3 - i] * srcVec2[3 - i] + srcVec3[3 - i];
		}
	}
}

void PICAShader::madi(u32 instruction) {
	const u32 operandDescriptor = operandDescriptors[instruction & 0x1f];
	const u32 src1 = getBits<17, 5>(instruction);
	const u32 src2 = getBits<12, 5>(instruction);
	u32 src3 = getBits<5, 7>(instruction);
	const u32 idx = getBits<22, 2>(instruction);
	const u32 dest = getBits<24, 5>(instruction);

	src3 = getIndexedSource(src3, idx);

	auto srcVec1 = getSourceSwizzled<1>(src1, operandDescriptor);
	auto srcVec2 = getSourceSwizzled<2>(src2, operandDescriptor);
	auto srcVec3 = getSourceSwizzled<3>(src3, operandDescriptor);
	auto& destVector = getDest(dest);

	u32 componentMask = operandDescriptor & 0xf;
	for (int i = 0; i < 4; i++) {
		if (componentMask & (1 << i)) {
			destVector[3 - i] = srcVec1[3 - i] * srcVec2[3 - i] + srcVec3[3 - i];
		}
	}
}

void PICAShader::slt(u32 instruction) {
	const u32 operandDescriptor = operandDescriptors[instruction & 0x7f];
	u32 src1 = getBits<12, 7>(instruction);
	const u32 src2 = getBits<7, 5>(instruction);  // src2 coming first because PICA moment
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	src1 = getIndexedSource(src1, idx);
	vec4f srcVec1 = getSourceSwizzled<1>(src1, operandDescriptor);
	vec4f srcVec2 = getSourceSwizzled<2>(src2, operandDescriptor);
	auto& destVector = getDest(dest);

	u32 componentMask = operandDescriptor & 0xf;
	for (int i = 0; i < 4; i++) {
		if (componentMask & (1 << i)) {
			destVector[3 - i] = srcVec1[3 - i] < srcVec2[3 - i] ? f24::fromFloat32(1.0) : f24::zero();
		}
	}
}

void PICAShader::sge(u32 instruction) {
	const u32 operandDescriptor = operandDescriptors[instruction & 0x7f];
	u32 src1 = getBits<12, 7>(instruction);
	const u32 src2 = getBits<7, 5>(instruction);  // src2 coming first because PICA moment
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	src1 = getIndexedSource(src1, idx);
	vec4f srcVec1 = getSourceSwizzled<1>(src1, operandDescriptor);
	vec4f srcVec2 = getSourceSwizzled<2>(src2, operandDescriptor);
	auto& destVector = getDest(dest);

	u32 componentMask = operandDescriptor & 0xf;
	for (int i = 0; i < 4; i++) {
		if (componentMask & (1 << i)) {
			destVector[3 - i] = srcVec1[3 - i] >= srcVec2[3 - i] ? f24::fromFloat32(1.0) : f24::zero();
		}
	}
}

void PICAShader::sgei(u32 instruction) {
	const u32 operandDescriptor = operandDescriptors[instruction & 0x7f];
	const u32 src1 = getBits<14, 5>(instruction);
	u32 src2 = getBits<7, 7>(instruction);
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	src2 = getIndexedSource(src2, idx);

	auto srcVec1 = getSourceSwizzled<1>(src1, operandDescriptor);
	auto srcVec2 = getSourceSwizzled<2>(src2, operandDescriptor);
	auto& destVector = getDest(dest);

	u32 componentMask = operandDescriptor & 0xf;
	for (int i = 0; i < 4; i++) {
		if (componentMask & (1 << i)) {
			destVector[3 - i] = srcVec1[3 - i] >= srcVec2[3 - i] ? f24::fromFloat32(1.0) : f24::zero();
		}
	}
}

void PICAShader::slti(u32 instruction) {
	const u32 operandDescriptor = operandDescriptors[instruction & 0x7f];
	const u32 src1 = getBits<14, 5>(instruction);
	u32 src2 = getBits<7, 7>(instruction);
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	src2 = getIndexedSource(src2, idx);

	auto srcVec1 = getSourceSwizzled<1>(src1, operandDescriptor);
	auto srcVec2 = getSourceSwizzled<2>(src2, operandDescriptor);
	auto& destVector = getDest(dest);

	u32 componentMask = operandDescriptor & 0xf;
	for (int i = 0; i < 4; i++) {
		if (componentMask & (1 << i)) {
			destVector[3 - i] = srcVec1[3 - i] < srcVec2[3 - i] ? f24::fromFloat32(1.0) : f24::zero();
		}
	}
}

void PICAShader::cmp(u32 instruction) {
	const u32 operandDescriptor = operandDescriptors[instruction & 0x7f];
	const u32 src1 = getBits<12, 7>(instruction);
	const u32 src2 = getBits<7, 5>(instruction);  // src2 coming first because PICA moment
	const u32 idx = getBits<19, 2>(instruction);
	const u32 cmpY = getBits<21, 3>(instruction);
	const u32 cmpX = getBits<24, 3>(instruction);
	const u32 cmpOperations[2] = {cmpX, cmpY};

	if (idx) Helpers::panic("[PICA] CMP: idx != 0");
	vec4f srcVec1 = getSourceSwizzled<1>(src1, operandDescriptor);
	vec4f srcVec2 = getSourceSwizzled<2>(src2, operandDescriptor);

	for (int i = 0; i < 2; i++) {
		switch (cmpOperations[i]) {
			case 0:  // Equal
				cmpRegister[i] = srcVec1[i] == srcVec2[i];
				break;

			case 1:  // Not equal
				cmpRegister[i] = srcVec1[i] != srcVec2[i];
				break;

			case 2:  // Less than
				cmpRegister[i] = srcVec1[i] < srcVec2[i];
				break;

			case 3:  // Less than or equal
				cmpRegister[i] = srcVec1[i] <= srcVec2[i];
				break;

			case 4:  // Greater than
				cmpRegister[i] = srcVec1[i] > srcVec2[i];
				break;

			case 5:  // Greater than or equal
				cmpRegister[i] = srcVec1[i] >= srcVec2[i];
				break;

			default: {
				cmpRegister[i] = true;
				break;
			}
		}
	}
}

void PICAShader::ifc(u32 instruction) {
	const u32 dest = getBits<10, 12>(instruction);

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

void PICAShader::ifu(u32 instruction) {
	const u32 dest = getBits<10, 12>(instruction);
	const u32 bit = getBits<22, 4>(instruction);  // Bit of the bool uniform to check

	if (boolUniform & (1 << bit)) {
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

void PICAShader::call(u32 instruction) {
	if (callIndex >= 4) [[unlikely]]
		Helpers::panic("[PICA] Overflowed CALL stack");

	const u32 num = instruction & 0xff;
	const u32 dest = getBits<10, 12>(instruction);

	auto& block = callInfo[callIndex++];
	block.endingPC = dest + num;
	block.returnPC = pc;

	pc = dest;
}

void PICAShader::callc(u32 instruction) {
	if (isCondTrue(instruction)) {
		call(instruction);  // Pls inline
	}
}

void PICAShader::callu(u32 instruction) {
	const u32 bit = getBits<22, 4>(instruction);  // Bit of the bool uniform to check

	if (boolUniform & (1 << bit)) {
		if (callIndex >= 4) [[unlikely]]
			Helpers::panic("[PICA] Overflowed CALL stack");

		const u32 num = instruction & 0xff;
		const u32 dest = getBits<10, 12>(instruction);

		auto& block = callInfo[callIndex++];
		block.endingPC = dest + num;
		block.returnPC = pc;

		pc = dest;
	}
}

void PICAShader::loop(u32 instruction) {
	if (loopIndex >= 4) [[unlikely]]
		Helpers::panic("[PICA] Overflowed loop stack");

	u32 dest = getBits<10, 12>(instruction);
	auto& uniform = intUniforms[getBits<22, 2>(instruction)];  // The uniform we'll get loop info from
	loopCounter = uniform[1];
	auto& loop = loopInfo[loopIndex++];

	loop.startingPC = pc;
	loop.endingPC = dest + 1;  // Loop is inclusive so we need + 1 here
	loop.iterations = uniform[0] + 1;
	loop.increment = uniform[2];
}

void PICAShader::jmpc(u32 instruction) {
	if (isCondTrue(instruction)) {
		pc = getBits<10, 12>(instruction);
	}
}

void PICAShader::jmpu(u32 instruction) {
	const u32 test = (instruction & 1) ^ 1;  // If the LSB is 0 we want to compare to true, otherwise compare to false
	const u32 dest = getBits<10, 12>(instruction);
	const u32 bit = getBits<22, 4>(instruction);  // Bit of the bool uniform to check

	if (((boolUniform >> bit) & 1) == test)  // Jump if the bool uniform is the value we want
		pc = dest;
}