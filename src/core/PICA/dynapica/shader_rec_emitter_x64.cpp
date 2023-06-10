#if defined(PANDA3DS_DYNAPICA_SUPPORTED) && defined(PANDA3DS_X64_HOST)
#include "PICA/dynapica/shader_rec_emitter_x64.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>

using namespace Xbyak;
using namespace Xbyak::util;
using namespace Helpers;

// Register that points to PICA state
static constexpr Reg64 statePointer = rbp;
static constexpr Xmm scratch1 = xmm0;
static constexpr Xmm scratch2 = xmm1;
static constexpr Xmm src1_xmm = xmm2;
static constexpr Xmm src2_xmm = xmm3;
static constexpr Xmm src3_xmm = xmm4;

void ShaderEmitter::compile(const PICAShader& shaderUnit) {
	// Constants
	align(16);
	L(negateVector);
	dd(0x80000000); dd(0x80000000); dd(0x80000000); dd(0x80000000); // -0.0 4 times

	// Emit prologue first
	align(16);
	prologueCb = getCurr<PrologueCallback>();

	// We assume arg1 contains the pointer to the PICA state and arg2 a pointer to the code for the entrypoint
	push(statePointer); // Back up state pointer to stack. This also aligns rsp to 16 bytes for calls
	mov(statePointer, (uintptr_t)&shaderUnit); // Set state pointer to the proper pointer

	// If we add integer register allocations they should be pushed here, and the rsp should be properly fixed up
	// However most of the PICA is floating point so yeah

	// Allocate shadow stack on Windows
	if constexpr (isWindows()) {
		sub(rsp, 32);
	}
	// Tail call to shader code entrypoint
	jmp(arg2);
	align(16);
	// Scan the shader code for call instructions and add them to the list of possible return PCs. We need to do this because the PICA callstack works
	// Pretty weirdly
	scanForCalls(shaderUnit);

	// Compile every instruction in the shader
	// This sounds horrible but the PICA instruction memory is tiny, and most of the time it's padded wtih nops that compile to nothing
	recompilerPC = 0;
	compileUntil(shaderUnit, PICAShader::maxInstructionCount);
}

void ShaderEmitter::scanForCalls(const PICAShader& shaderUnit) {
	returnPCs.clear();

	for (u32 i = 0; i < PICAShader::maxInstructionCount; i++) {
		const u32 instruction = shaderUnit.loadedShader[i];
		if (isCall(instruction)) {
			const u32 num = instruction & 0xff;
			const u32 dest = getBits<10, 12>(instruction);
			const u32 returnPC = num + dest; // Add them to get the return PC

			returnPCs.push_back(returnPC);
		}
	}

	// Sort return PCs so they can be binary searched
	std::sort(returnPCs.begin(), returnPCs.end());
}

void ShaderEmitter::compileUntil(const PICAShader& shaderUnit, u32 end) {
	while (recompilerPC < end) {
		compileInstruction(shaderUnit);
	}
}

void ShaderEmitter::compileInstruction(const PICAShader& shaderUnit) {
	// Write current location to label for this instruction
	L(instructionLabels[recompilerPC]);

	// See if PC is a possible return PC and emit the proper code if so
	if (std::binary_search(returnPCs.begin(), returnPCs.end(), recompilerPC)) {
		// This is the offset we need to add to rsp to peek the next return address in the callstack
		// Ie the PICA PC address which, when reached, will trigger a return
		const auto stackOffsetForPC = isWindows() ? (16 + 32) : 16;
		
		Label end;
		// Check if return address == recompilerPC, ie if we should return
		cmp(dword[rsp + stackOffsetForPC], recompilerPC);
		jne(end); // If not, continue with uor lives
		ret();    // If yes, return

		L(end);
	}

	// Fetch instruction and inc PC
	const u32 instruction = shaderUnit.loadedShader[recompilerPC++];
	const u32 opcode = instruction >> 26;

	switch (opcode) {
		case ShaderOpcodes::ADD: recADD(shaderUnit, instruction); break;
		case ShaderOpcodes::CALL:
			recCALL(shaderUnit, instruction);
			break;
		case ShaderOpcodes::CMP1: case ShaderOpcodes::CMP2:
			recCMP(shaderUnit, instruction);
			break;
		case ShaderOpcodes::DP3: recDP3(shaderUnit, instruction); break;
		case ShaderOpcodes::DP4: recDP4(shaderUnit, instruction); break;
		case ShaderOpcodes::END: recEND(shaderUnit, instruction); break;
		case ShaderOpcodes::IFC: recIFC(shaderUnit, instruction); break;
		case ShaderOpcodes::IFU: recIFU(shaderUnit, instruction); break;
		case ShaderOpcodes::MOV: recMOV(shaderUnit, instruction); break;
		case ShaderOpcodes::MOVA: recMOVA(shaderUnit, instruction); break;
		case ShaderOpcodes::MAX: recMAX(shaderUnit, instruction); break;
		case ShaderOpcodes::MUL: recMUL(shaderUnit, instruction); break;
		case ShaderOpcodes::NOP: break;
		case ShaderOpcodes::RCP: recRCP(shaderUnit, instruction); break;
		case ShaderOpcodes::RSQ: recRSQ(shaderUnit, instruction); break;

		case 0x38: case 0x39: case 0x3A: case 0x3B: case 0x3C: case 0x3D: case 0x3E: case 0x3F:
			recMAD(shaderUnit, instruction);
			break;
		default:
			Helpers::panic("Shader JIT: Unimplemented PICA opcode %X", opcode);
	}
}

const ShaderEmitter::vec4f& ShaderEmitter::getSourceRef(const PICAShader& shader, u32 src) {
	if (src < 0x10)
		return shader.inputs[src];
	else if (src < 0x20)
		return shader.tempRegisters[src - 0x10];
	else if (src <= 0x7f)
		return shader.floatUniforms[src - 0x20];
	else {
		Helpers::warn("[Shader JIT] Unimplemented source value: %X\n", src);
		return shader.dummy;
	}
}

const ShaderEmitter::vec4f& ShaderEmitter::getDestRef(const PICAShader& shader, u32 dest) {
	if (dest < 0x10) {
		return shader.outputs[dest];
	} else if (dest < 0x20) {
		return shader.tempRegisters[dest - 0x10];
	}
	Helpers::panic("[Shader JIT] Unimplemented dest: %X", dest);
}

// See shader.hpp header for docs on how the swizzle and negate works
template <int sourceIndex>
void ShaderEmitter::loadRegister(Xmm dest, const PICAShader& shader, u32 src, u32 index, u32 operandDescriptor) {
	u32 compSwizzle; // Component swizzle pattern for the register
	bool negate;     // If true, negate all lanes of the register

	if constexpr (sourceIndex == 1) { // SRC1
		negate = (getBit<4>(operandDescriptor)) != 0;
		compSwizzle = getBits<5, 8>(operandDescriptor);
	}
	else if constexpr (sourceIndex == 2) { // SRC2
		negate = (getBit<13>(operandDescriptor)) != 0;
		compSwizzle = getBits<14, 8>(operandDescriptor);
	}
	else if constexpr (sourceIndex == 3) { // SRC3
		negate = (getBit<22>(operandDescriptor)) != 0;
		compSwizzle = getBits<23, 8>(operandDescriptor);
	}

	// PICA has the swizzle descriptor inverted in comparison to x86. For the PICA, the descriptor is (lowest to highest bits) wzyx while it's xyzw for x86
	u32 convertedSwizzle = ((compSwizzle >> 6) & 0b11) | (((compSwizzle >> 4) & 0b11) << 2) | (((compSwizzle >> 2) & 0b11) << 4) | ((compSwizzle & 0b11) << 6);

	switch (index) {
		case 0: [[likely]] { // Keep src as is, no need to offset it
			const vec4f& srcRef = getSourceRef(shader, src);
			const uintptr_t offset = uintptr_t(&srcRef) - uintptr_t(&shader); // Calculate offset of register from start of the state struct

			if (compSwizzle == noSwizzle) // Avoid emitting swizzle if not necessary
				movaps(dest, xword[statePointer + offset]);
			else // Swizzle is not trivial so we need to emit a shuffle instruction
				pshufd(dest, xword[statePointer + offset], convertedSwizzle);

			// Negate the register if necessary
			if (negate) {
				pxor(dest, xword[rip + negateVector]);
			}
			return; // Return. Rest of the function handles indexing which is not used if index == 0
		}

		case 1: {
			const uintptr_t addrXOffset = uintptr_t(&shader.addrRegister[0]) - uintptr_t(&shader);
			movsxd(rax, dword[statePointer + addrXOffset]); // rax = address register x
			break;
		}

		case 2: {
			const uintptr_t addrYOffset = uintptr_t(&shader.addrRegister[1]) - uintptr_t(&shader);
			movsxd(rax, dword[statePointer + addrYOffset]); // rax = address register y
			break;
		}
		
		default:
			Helpers::panic("[ShaderJIT]: Unimplemented source index type %d", index);
	}

	// Here we handle what happens when using indexed addressing & we can't predict what register will be read at compile time
	// The index of the access is assumed to be in rax
	// Add source register (src) and index (rax) to form the final register
	add(rax, src);

	Label maybeTemp, maybeUniform, unknownReg, end;
	const uintptr_t inputOffset = uintptr_t(&shader.inputs) - uintptr_t(&shader);
	const uintptr_t tempOffset = uintptr_t(&shader.tempRegisters) - uintptr_t(&shader);
	const uintptr_t uniformOffset = uintptr_t(&shader.floatUniforms) - uintptr_t(&shader);

	// If reg < 0x10, return inputRegisters[reg]
	cmp(rax, 0x10);
	jae(maybeTemp);
	mov(rcx, rax);
	shl(rcx, 4);  // rcx = rax * sizeof(vec4 of floats) = rax * 16
	movaps(dest, xword[statePointer + rcx + inputOffset]);
	jmp(end);
	
	// If (reg < 0x1F) return tempRegisters[reg - 0x10]
	L(maybeTemp);
	cmp(rax, 0x20);
	jae(maybeUniform);
	lea(rcx, qword[rax - 0x10]);
	shl(rcx, 4);
	movaps(dest, xword[statePointer + rcx + tempOffset]);
	jmp(end);

	// If (reg < 0x80) return floatUniforms[reg - 0x20]
	L(maybeUniform);
	cmp(rax, 0x80);
	jae(unknownReg);
	lea(rcx, qword[rax - 0x20]);
	shl(rcx, 4);
	movaps(dest, xword[statePointer + rcx + uniformOffset]);
	jmp(end);

	L(unknownReg);
	pxor(dest, dest); // Set dest to 0 if we're reading from a garbage register

	L(end);
	// Negate the register if necessary
	if (negate) {
		pxor(dest, xword[rip + negateVector]);
	}
}

void ShaderEmitter::storeRegister(Xmm source, const PICAShader& shader, u32 dest, u32 operandDescriptor) {
	const vec4f& destRef = getDestRef(shader, dest);
	const uintptr_t offset = uintptr_t(&destRef) - uintptr_t(&shader); // Calculate offset of register from start of the state struct

	// Mask of which lanes to write
	u32 writeMask = operandDescriptor & 0xf;
	if (writeMask == 0xf) { // No lanes are masked, just movaps
		movaps(xword[statePointer + offset], source);
	} else if (std::popcount(writeMask) == 1) { // Only 1 register needs to be written back. This can be done with a simple shift right + movss
		int bit = std::countr_zero(writeMask); // Get which PICA register needs to be written to (0 = w, 1 = z, etc)
		size_t index = 3 - bit;
		const uintptr_t lane_offset = offset + index * sizeof(float);

		if (index == 0) { // Bottom lane, no need to shift
			movss(dword[statePointer + lane_offset], source);
		} else { // Shift right by 32 * index, then write bottom lane
			movaps(scratch1, source);
			psrldq(scratch1, index * sizeof(float));
			movss(dword[statePointer + lane_offset], scratch1);
		}
	} else if (haveSSE4_1) {
		// Bit reverse the write mask because that is what blendps expects
		u32 adjustedMask = ((writeMask >> 3) & 0b1) | ((writeMask >> 1) & 0b10) | ((writeMask << 1) & 0b100) | ((writeMask << 3) & 0b1000);
		movaps(scratch1, xword[statePointer + offset]); // Read current value of dest
		blendps(scratch1, source, adjustedMask);        // Blend with source
		movaps(xword[statePointer + offset], scratch1); // Write back
	} else {
		// Blend algo referenced from Citra
		const u8 selector = (((writeMask & 0b1000) ? 1 : 0) << 0) |
			(((writeMask & 0b0100) ? 3 : 2) << 2) |
			(((writeMask & 0b0010) ? 0 : 1) << 4) |
			(((writeMask & 0b0001) ? 2 : 3) << 6);

		movaps(scratch1, xword[statePointer + offset]);
		movaps(scratch2, source);
		unpckhps(scratch2, scratch1); // Unpack X/Y components of source and destination
		unpcklps(scratch1, source);   // Unpack Z/W components of source and destination
		shufps(scratch1, scratch2, selector); // "merge-shuffle" dest and source using selecto
		movaps(xword[statePointer + offset], scratch1); // Write back
	}
}

void ShaderEmitter::checkCmpRegister(const PICAShader& shader, u32 instruction) {
	static_assert(sizeof(bool) == 1 && sizeof(shader.cmpRegister) == 2); // The code below relies on bool being 1 byte exactly
	const size_t cmpRegXOffset = uintptr_t(&shader.cmpRegister) - uintptr_t(&shader);
	const size_t cmpRegYOffset = cmpRegXOffset + sizeof(bool);

	const u32 condition = getBits<22, 2>(instruction);
	const uint refY = getBit<24>(instruction);
	const uint refX = getBit<25>(instruction);

	// refX in the bottom byte, refY in the top byte. This is done for condition codes 0 and 1 which check both x and y, so we can emit a single instruction that checks both
	const u16 refX_refY_merged = refX | (refY << 8);

	switch (condition) {
		case 0: // Either cmp register matches 
			// Z flag is 0 if at least 1 of them is set
			test(word[statePointer + cmpRegXOffset], refX_refY_merged);

			// Invert z flag
			setz(al);
			test(al, al);
			break;
		case 1: // Both cmp registers match
			cmp(word[statePointer + cmpRegXOffset], refX_refY_merged);
			break;
		case 2: // At least cmp.x matches
			cmp(byte[statePointer + cmpRegXOffset], refX);
			break;
		default: // At least cmp.y matches
			cmp(byte[statePointer + cmpRegYOffset], refY);
			break;
	}
}

void ShaderEmitter::checkBoolUniform(const PICAShader& shader, u32 instruction) {
	const u32 bit = getBits<22, 4>(instruction); // Bit of the bool uniform to check
	const uintptr_t boolUniformOffset = uintptr_t(&shader.boolUniform) - uintptr_t(&shader);

	test(word[statePointer + boolUniformOffset], 1 << bit);
}

void ShaderEmitter::recEND(const PICAShader& shader, u32 instruction) {
	// Undo anything the prologue did and return
	// Dellocate shadow stack on Windows
	if constexpr (isWindows()) {
		add(rsp, 32);
	}

	// Restore registers
	pop(statePointer);
	ret();
}

void ShaderEmitter::recMOV(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	const u32 src = getBits<12, 7>(instruction);
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	loadRegister<1>(src1_xmm, shader, src, idx, operandDescriptor); // Load source 1 into scratch1
	storeRegister(src1_xmm, shader, dest, operandDescriptor);
}

void ShaderEmitter::recMOVA(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	const u32 src = getBits<12, 7>(instruction);
	const u32 idx = getBits<19, 2>(instruction);

	const bool writeX = getBit<3>(operandDescriptor); // Should we write the x component of the address register?
	const bool writeY = getBit<2>(operandDescriptor);

	static_assert(sizeof(shader.addrRegister) == 2 * sizeof(s32)); // Assert that the address register is 2 s32s
	const uintptr_t addrRegisterOffset = uintptr_t(&shader.addrRegister[0]) - uintptr_t(&shader);
	const uintptr_t addrRegisterYOffset = addrRegisterOffset + sizeof(shader.addrRegister[0]);

	// If no register is being written to then it is a nop. Probably not common but whatever
	if (!writeX && !writeY) return;
	loadRegister<1>(src1_xmm, shader, src, idx, operandDescriptor); // Load source 1 into scratch1

	// Write both
	if (writeX && writeY) {
		cvttps2dq(scratch1, src1_xmm); // Convert all lanes of src1 with truncation
		movsd(qword[statePointer + addrRegisterOffset], scratch1); // Write back bottom 2 to addr register x and ys
	}
	else if (writeX) {
		cvttss2si(scratch1, src1_xmm); // Convert bottom lane
		movss(dword[statePointer + addrRegisterOffset], scratch1); // Write it back
	}
	else if (writeY) {
		psrldq(src1_xmm, sizeof(float)); // Shift y component to bottom lane
		cvttss2si(scratch1, src1_xmm); // Convert bottom lane
		movss(dword[statePointer + addrRegisterYOffset], scratch1); // Write it back to y component
	}
}

void ShaderEmitter::recADD(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	const u32 src1 = getBits<12, 7>(instruction);
	const u32 src2 = getBits<7, 5>(instruction); // src2 coming first because PICA moment
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	loadRegister<1>(src1_xmm, shader, src1, idx, operandDescriptor);
	loadRegister<2>(src2_xmm, shader, src2, 0, operandDescriptor);
	addps(src1_xmm, src2_xmm); // Dot product between the 2 register
	storeRegister(src1_xmm, shader, dest, operandDescriptor);
}

void ShaderEmitter::recDP3(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	const u32 src1 = getBits<12, 7>(instruction);
	const u32 src2 = getBits<7, 5>(instruction); // src2 coming first because PICA moment
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	// TODO: Safe multiplication equivalent (Multiplication is not IEEE compliant on the PICA)
	loadRegister<1>(src1_xmm, shader, src1, idx, operandDescriptor);
	loadRegister<2>(src2_xmm, shader, src2, 0, operandDescriptor);
	dpps(src1_xmm, src2_xmm, 0b01111111); // 3-lane dot product between the 2 registers, store the result in all lanes of scratch1 similarly to PICA 
	storeRegister(src1_xmm, shader, dest, operandDescriptor);
}

void ShaderEmitter::recDP4(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	const u32 src1 = getBits<12, 7>(instruction);
	const u32 src2 = getBits<7, 5>(instruction); // src2 coming first because PICA moment
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	// TODO: Safe multiplication equivalent (Multiplication is not IEEE compliant on the PICA)
	loadRegister<1>(src1_xmm, shader, src1, idx, operandDescriptor);
	loadRegister<2>(src2_xmm, shader, src2, 0, operandDescriptor);
	dpps(src1_xmm, src2_xmm, 0b11111111); // 4-lane dot product between the 2 registers, store the result in all lanes of scratch1 similarly to PICA 
	storeRegister(src1_xmm, shader, dest, operandDescriptor);
}

void ShaderEmitter::recMAX(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	const u32 src1 = getBits<12, 7>(instruction);
	const u32 src2 = getBits<7, 5>(instruction); // src2 coming first because PICA moment
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	loadRegister<1>(src1_xmm, shader, src1, idx, operandDescriptor);
	loadRegister<2>(src2_xmm, shader, src2, 0, operandDescriptor);
	maxps(src1_xmm, src2_xmm);
	storeRegister(src1_xmm, shader, dest, operandDescriptor);
}

void ShaderEmitter::recMUL(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	const u32 src1 = getBits<12, 7>(instruction);
	const u32 src2 = getBits<7, 5>(instruction); // src2 coming first because PICA moment
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	// TODO: Safe multiplication equivalent (Multiplication is not IEEE compliant on the PICA)
	loadRegister<1>(src1_xmm, shader, src1, idx, operandDescriptor);
	loadRegister<2>(src2_xmm, shader, src2, 0, operandDescriptor);
	mulps(src1_xmm, src2_xmm);
	storeRegister(src1_xmm, shader, dest, operandDescriptor);
}

void ShaderEmitter::recRCP(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	const u32 src = getBits<12, 7>(instruction);
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);
	const u32 writeMask = operandDescriptor & 0xf;

	loadRegister<1>(src1_xmm, shader, src, idx, operandDescriptor); // Load source 1 into scratch1
	rcpss(src1_xmm, src1_xmm); // Compute rcp approximation

	// If we only write back the x component to the result, we needn't perform a shuffle to do res = res.xxxx
	// Otherwise we do
	if (writeMask != 0x8) {// Copy bottom lane to all lanes if we're not simply writing back x
		shufps(src1_xmm, src1_xmm, 0); // src1_xmm = src1_xmm.xxxx
	}

	storeRegister(src1_xmm, shader, dest, operandDescriptor);
}

void ShaderEmitter::recRSQ(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	const u32 src = getBits<12, 7>(instruction);
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);
	const u32 writeMask = operandDescriptor & 0xf;

	loadRegister<1>(src1_xmm, shader, src, idx, operandDescriptor); // Load source 1 into scratch1
	rsqrtss(src1_xmm, src1_xmm); // Compute rsqrt approximation

	// If we only write back the x component to the result, we needn't perform a shuffle to do res = res.xxxx
	// Otherwise we do
	if (writeMask != 0x8) {// Copy bottom lane to all lanes if we're not simply writing back x
		shufps(src1_xmm, src1_xmm, 0); // src1_xmm = src1_xmm.xxxx
	}

	storeRegister(src1_xmm, shader, dest, operandDescriptor);
}

void ShaderEmitter::recMAD(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x1f];
	const u32 src1 = getBits<17, 5>(instruction);
	const u32 src2 = getBits<10, 7>(instruction);
	const u32 src3 = getBits<5, 5>(instruction);
	const u32 idx = getBits<22, 2>(instruction);
	const u32 dest = getBits<24, 5>(instruction);

	loadRegister<1>(src1_xmm, shader, src1, 0, operandDescriptor);
	loadRegister<2>(src2_xmm, shader, src2, idx, operandDescriptor);
	loadRegister<3>(src3_xmm, shader, src3, 0, operandDescriptor);

	movaps(scratch1, src1_xmm);
	// TODO: Implement safe PICA mul
	mulps(scratch1, src2_xmm);
	addps(scratch1, src3_xmm);
	storeRegister(scratch1, shader, dest, operandDescriptor);
}

void ShaderEmitter::recCMP(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	const u32 src1 = getBits<12, 7>(instruction);
	const u32 src2 = getBits<7, 5>(instruction); // src2 coming first because PICA moment
	const u32 idx = getBits<19, 2>(instruction);
	const u32 cmpY = getBits<21, 3>(instruction);
	const u32 cmpX = getBits<24, 3>(instruction);

	loadRegister<1>(src1_xmm, shader, src1, idx, operandDescriptor);
	loadRegister<2>(src2_xmm, shader, src2, 0, operandDescriptor);

	// Condition codes for cmpps
	enum : u8 {
		CMP_EQ = 0,
		CMP_LT = 1,
		CMP_LE = 2,
		CMP_UNORD = 3,
		CMP_NEQ = 4,
		CMP_NLT = 5,
		CMP_NLE = 6,
		CMP_ORD = 7,
		CMP_TRUE = 15
	};

	// Map from PICA condition codes (used as index) to x86 condition codes
	static constexpr std::array<u8, 8> conditionCodes = { CMP_EQ, CMP_NEQ, CMP_LT, CMP_LE, CMP_LT, CMP_LE, CMP_TRUE, CMP_TRUE };

	// SSE does not offer GT or GE comparisons in the cmpps instruction, so we need to flip the left and right operands in that case and use LT/LE
	const bool invertX = (cmpX == 4 || cmpX == 5);
	const bool invertY = (cmpY == 4 || cmpY == 5);
	Xmm lhs_x = invertX ? src2_xmm : src1_xmm;
	Xmm rhs_x = invertX ? src1_xmm : src2_xmm;
	Xmm lhs_y = invertY ? src2_xmm : src1_xmm;
	Xmm rhs_y = invertY ? src1_xmm : src2_xmm;

	const u8 compareFuncX = conditionCodes[cmpX];
	const u8 compareFuncY = conditionCodes[cmpY];

	static_assert(sizeof(bool) == 1 && sizeof(shader.cmpRegister) == 2); // The code below relies on bool being 1 byte exactly
	const size_t cmpRegXOffset = uintptr_t(&shader.cmpRegister) - uintptr_t(&shader);
	const size_t cmpRegYOffset = cmpRegXOffset + sizeof(bool);

	// Cmp x and y are the same compare function, we can use a single cmp instruction
	if (cmpX == cmpY) {
		cmpps(lhs_x, rhs_x, compareFuncX);
		movd(eax, lhs_x);
		test(eax, eax);

		setne(byte[statePointer + cmpRegXOffset]);
		setne(byte[statePointer + cmpRegYOffset]);
	} else {
		movaps(scratch1, lhs_x); // Copy the left hand operands to temp registers
		movaps(scratch2, lhs_y);

		cmpps(scratch1, rhs_x, compareFuncX); // Perform the compares
		cmpps(scratch2, rhs_y, compareFuncY);

		movd(eax, scratch1); // Move results to eax for X and edx for Y
		movd(edx, scratch2);

		test(eax, eax);      // Write back results with setne
		setne(byte[statePointer + cmpRegXOffset]);
		test(edx, edx);
		setne(byte[statePointer + cmpRegYOffset]);
	}
}

void ShaderEmitter::recIFC(const PICAShader& shader, u32 instruction) {
	// z is 1 if true, else 0
	checkCmpRegister(shader, instruction);
	const u32 num = instruction & 0xff;
	const u32 dest = getBits<10, 12>(instruction);

	if (dest < recompilerPC) {
		Helpers::warn("Shader JIT: IFC instruction with dest < current PC\n");
	}
	Label elseBlock, endIf;

	// Jump to else block if z is 0
	jnz(elseBlock, T_NEAR);
	compileUntil(shader, dest);

	if (num == 0) { // Else block is empty,
		L(elseBlock);
	} else { // Else block is NOT empty
		jmp(endIf, T_NEAR);
		L(elseBlock);
		compileUntil(shader, dest + num);
		L(endIf);
	}
}

void ShaderEmitter::recIFU(const PICAShader& shader, u32 instruction) {
	// z is 0 if true, else 1
	checkBoolUniform(shader, instruction);
	const u32 num = instruction & 0xff;
	const u32 dest = getBits<10, 12>(instruction);

	if (dest < recompilerPC) {
		Helpers::warn("Shader JIT: IFC instruction with dest < current PC\n");
	}
	Label elseBlock, endIf;

	// Jump to else block if z is 1
	jz(elseBlock, T_NEAR);
	compileUntil(shader, dest);

	if (num == 0) { // Else block is empty,
		L(elseBlock);
	}
	else { // Else block is NOT empty
		jmp(endIf, T_NEAR);
		L(elseBlock);
		compileUntil(shader, dest + num);
		L(endIf);
	}
}

void ShaderEmitter::recCALL(const PICAShader& shader, u32 instruction) {
	const u32 num = instruction & 0xff;
	const u32 dest = getBits<10, 12>(instruction);

	// Push return PC as stack parameter. This is a decently fast solution and Citra does the same but we should probably switch to a proper PICA-like
	// Callstack, because it's not great to have an infinitely expanding call stack where popping from empty stack is undefined as hell
	push(qword, dest + num);
	// Realign stack to 64 bits and allocate 32 bytes of shadow space on windows
	sub(rsp, isWindows() ? (8 + 32) : 8);

	// Call subroutine, Xbyak will update the label if it hasn't been initialized yet
	call(instructionLabels[dest]);

	// Fix up stack after returning. The 8 from before becomes a 16 because we're also skipping the qword we pushed
	add(rsp, isWindows() ? (16 + 32) : 16);
}

#endif