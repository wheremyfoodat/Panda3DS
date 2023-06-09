#if defined(PANDA3DS_DYNAPICA_SUPPORTED) && defined(PANDA3DS_X64_HOST)
#include "PICA/dynapica/shader_rec_emitter_x64.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>

using namespace Xbyak;
using namespace Xbyak::util;

// Register that points to PICA state
static constexpr Reg64 statePointer = rbp;
static constexpr Xmm scratch1 = xmm0;
static constexpr Xmm scratch2 = xmm1;
static constexpr Xmm src1_xmm = xmm2;
static constexpr Xmm src2_xmm = xmm3;
static constexpr Xmm src3_xmm = xmm4;

void ShaderEmitter::compile(const PICAShader& shaderUnit) {
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
			const u32 num = instruction & 0xff; // Num of instructions to execute
			const u32 dest = (instruction >> 10) & 0xfff; // Starting subroutine address
			const u32 returnPC = num + dest; // Add them to get the return PC

			returnPCs.push_back(returnPC);
		}
	}
}

void ShaderEmitter::compileUntil(const PICAShader& shaderUnit, u32 end) {
	while (recompilerPC < end) {
		compileInstruction(shaderUnit);
	}
}

void ShaderEmitter::compileInstruction(const PICAShader& shaderUnit) {
	// Write current location to label for this instruction
	L(instructionLabels[recompilerPC]);
	// Fetch instruction and inc PC
	const u32 instruction = shaderUnit.loadedShader[recompilerPC++];
	const u32 opcode = instruction >> 26;

	switch (opcode) {
		case ShaderOpcodes::ADD: recADD(shaderUnit, instruction); break;
		case ShaderOpcodes::CMP1: case ShaderOpcodes::CMP2:
			recCMP(shaderUnit, instruction);
			break;
		case ShaderOpcodes::DP4: recDP4(shaderUnit, instruction); break;
		case ShaderOpcodes::END: recEND(shaderUnit, instruction); break;
		case ShaderOpcodes::MOV: recMOV(shaderUnit, instruction); break;
		case ShaderOpcodes::NOP: break;
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
		negate = ((operandDescriptor >> 4) & 1) != 0;
		compSwizzle = (operandDescriptor >> 5) & 0xff;
	}
	else if constexpr (sourceIndex == 2) { // SRC2
		negate = ((operandDescriptor >> 13) & 1) != 0;
		compSwizzle = (operandDescriptor >> 14) & 0xff;
	}
	else if constexpr (sourceIndex == 3) { // SRC3
		negate = ((operandDescriptor >> 22) & 1) != 0;
		compSwizzle = (operandDescriptor >> 23) & 0xff;
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
			break;
		}
		
		default:
			Helpers::panic("[ShaderJIT]: Unimplemented source index type");
	}

	if (negate) {
		Helpers::panic("[ShaderJIT] Unimplemented register negation");
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
	u32 src = (instruction >> 12) & 0x7f;
	const u32 idx = (instruction >> 19) & 3;
	const u32 dest = (instruction >> 21) & 0x1f;

	loadRegister<1>(src1_xmm, shader, src, idx, operandDescriptor); // Load source 1 into scratch1
	storeRegister(src1_xmm, shader, dest, operandDescriptor);
}

void ShaderEmitter::recADD(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	u32 src1 = (instruction >> 12) & 0x7f;
	const u32 src2 = (instruction >> 7) & 0x1f; // src2 coming first because PICA moment
	const u32 idx = (instruction >> 19) & 3;
	const u32 dest = (instruction >> 21) & 0x1f;

	loadRegister<1>(src1_xmm, shader, src1, idx, operandDescriptor);
	loadRegister<2>(src2_xmm, shader, src2, 0, operandDescriptor);
	addps(src1_xmm, src2_xmm); // Dot product between the 2 register
	storeRegister(src1_xmm, shader, dest, operandDescriptor);
}

void ShaderEmitter::recDP4(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	u32 src1 = (instruction >> 12) & 0x7f;
	const u32 src2 = (instruction >> 7) & 0x1f; // src2 coming first because PICA moment
	const u32 idx = (instruction >> 19) & 3;
	const u32 dest = (instruction >> 21) & 0x1f;

	// TODO: Safe multiplication equivalent (Multiplication is not IEEE compliant on the PICA)
	loadRegister<1>(src1_xmm, shader, src1, idx, operandDescriptor);
	loadRegister<2>(src2_xmm, shader, src2, 0, operandDescriptor);
	dpps(src1_xmm, src2_xmm, 0b11111111); // Dot product between the 2 register, store the result in all lanes of scratch1 similarly to PICA 
	storeRegister(src1_xmm, shader, dest, operandDescriptor);
}

void ShaderEmitter::recCMP(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	const u32 src1 = (instruction >> 12) & 0x7f;
	const u32 src2 = (instruction >> 7) & 0x1f; // src2 coming first because PICA moment
	const u32 idx = (instruction >> 19) & 3;
	const u32 cmpY = (instruction >> 21) & 7;
	const u32 cmpX = (instruction >> 24) & 7;

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
	const size_t cmpRegYOffset = cmpRegXOffset + 1;

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

#endif