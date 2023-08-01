#if defined(PANDA3DS_DYNAPICA_SUPPORTED) && defined(PANDA3DS_X64_HOST)
#include "PICA/dynapica/shader_rec_emitter_x64.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <immintrin.h>
#include <smmintrin.h>

using namespace Xbyak;
using namespace Xbyak::util;
using namespace Helpers;

// The shader recompiler uses quite an odd internal ABI
// We make use of the fact that in regular conditions, we should pretty much never be calling C++ code from recompiled shader code
// This allows us to establish an ABI that's optimized for this sort of workflow, statically allocating volatile host registers
// To avoid pushing/popping registers, not properly maintaining stack alignment, etc
// This generates faster recompiled code at the cost of being actively hostile against C++ interop
// To do C++ interop, we're going to have our callCppFunc function call the C++ function, and take extreme measures to ensure we don't violate
// The host ABI, such as pushing/popping our temporary registers (derp), force aligning the stack and setting up an entire stack frame, etc
// This is slow, but we do not care as we should never be calling C++ code in normal, non-debugging conditions
// The only code that might be called are helpers that are also written in assembly, for things like log2

static constexpr Xmm scratch1 = xmm0;
static constexpr Xmm scratch2 = xmm1;
static constexpr Xmm src1_xmm = xmm2;
static constexpr Xmm src2_xmm = xmm3;
static constexpr Xmm src3_xmm = xmm4;

#if defined(PANDA3DS_MS_ABI)
// Register that points to PICA state. Must be volatile for the aforementioned reasons
static constexpr Reg64 statePointer = r8;
#elif defined(PANDA3DS_SYSV_ABI)
static constexpr Reg64 statePointer = rdi;
#else
#error Unknown ABI for x86-64 shader JIT
#endif

void ShaderEmitter::compile(const PICAShader& shaderUnit) {
	// Constants
	align(16);
	L(negateVector);
	dd(0x80000000); dd(0x80000000); dd(0x80000000); dd(0x80000000); // -0.0 4 times
	L(onesVector);
	dd(0x3f800000); dd(0x3f800000); dd(0x3f800000); dd(0x3f800000); // 1.0 4 times

	// Emit prologue first
	align(16);
	prologueCb = getCurr<PrologueCallback>();

	// Set state pointer to the proper pointer
	// state pointer is volatile, no need to preserve it
	mov(statePointer, arg1.cvt64());

	// Push a return guard on the stack. This happens due to the way we handle the PICA callstack, by pushing the return PC to stack
	// By pushing ffff'ffff, we make it impossible for a return check to erroneously pass
	push(qword, 0xffffffff);
	// Lower rsp by 8 for the PICA return stack to be consistent
	sub(rsp, 8);

	// Tail call to shader code entrypoint
	jmp(arg2);

	// Scan the code for call, exp2, log2, etc instructions which need some special care
	// After that, emit exp2 and log2 functions if the corresponding instructions are present
	scanCode(shaderUnit);
	if (codeHasExp2) exp2Func = emitExp2Func();
	if (codeHasLog2) log2Func = emitLog2Func();

	align(16);
	// Compile every instruction in the shader
	// This sounds horrible but the PICA instruction memory is tiny, and most of the time it's padded wtih nops that compile to nothing
	recompilerPC = 0;
	loopLevel = 0;
	compileUntil(shaderUnit, PICAShader::maxInstructionCount);
}

void ShaderEmitter::scanCode(const PICAShader& shaderUnit) {
	returnPCs.clear();

	for (u32 i = 0; i < PICAShader::maxInstructionCount; i++) {
		const u32 instruction = shaderUnit.loadedShader[i];
		const u32 opcode = instruction >> 26;

		if (isCall(instruction)) {
			const u32 num = instruction & 0xff;
			const u32 dest = getBits<10, 12>(instruction);
			const u32 returnPC = num + dest; // Add them to get the return PC

			returnPCs.push_back(returnPC);
		} else if (opcode == ShaderOpcodes::EX2) {
			codeHasExp2 = true;
		} else if (opcode == ShaderOpcodes::LG2) {
			codeHasLog2 = true;
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
		constexpr uintptr_t stackOffsetForPC = 8;

		Label end;
		// Check if return address == recompilerPC, ie if we should return
		cmp(dword[rsp + stackOffsetForPC], recompilerPC);
		jne(end);  // If not, continue with our lives
		ret();     // If yes, return

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
		case ShaderOpcodes::CALLC:
			recCALLC(shaderUnit, instruction);
			break;
		case ShaderOpcodes::CALLU:
			recCALLU(shaderUnit, instruction);
			break;
		case ShaderOpcodes::CMP1: case ShaderOpcodes::CMP2:
			recCMP(shaderUnit, instruction);
			break;
		case ShaderOpcodes::DP3: recDP3(shaderUnit, instruction); break;
		case ShaderOpcodes::DP4: recDP4(shaderUnit, instruction); break;
		case ShaderOpcodes::DPH: recDPH(shaderUnit, instruction); break;
		case ShaderOpcodes::END: recEND(shaderUnit, instruction); break;
		case ShaderOpcodes::EX2: recEX2(shaderUnit, instruction); break;
		case ShaderOpcodes::FLR: recFLR(shaderUnit, instruction); break;
		case ShaderOpcodes::IFC: recIFC(shaderUnit, instruction); break;
		case ShaderOpcodes::IFU: recIFU(shaderUnit, instruction); break;
		case ShaderOpcodes::JMPC: recJMPC(shaderUnit, instruction); break;
		case ShaderOpcodes::JMPU: recJMPU(shaderUnit, instruction); break;
		case ShaderOpcodes::LG2: recLG2(shaderUnit, instruction); break;
		case ShaderOpcodes::LOOP: recLOOP(shaderUnit, instruction); break;
		case ShaderOpcodes::MOV: recMOV(shaderUnit, instruction); break;
		case ShaderOpcodes::MOVA: recMOVA(shaderUnit, instruction); break;
		case ShaderOpcodes::MAX: recMAX(shaderUnit, instruction); break;
		case ShaderOpcodes::MIN: recMIN(shaderUnit, instruction); break;
		case ShaderOpcodes::MUL: recMUL(shaderUnit, instruction); break;
		case ShaderOpcodes::NOP: break;
		case ShaderOpcodes::RCP: recRCP(shaderUnit, instruction); break;
		case ShaderOpcodes::RSQ: recRSQ(shaderUnit, instruction); break;

		// Unimplemented opcodes that don't seem to actually be used but exist in the binary
		// EMIT/SETEMIT are used in geometry shaders, however are sometimes found in vertex shaders?
		case ShaderOpcodes::EMIT:
		case ShaderOpcodes::SETEMIT:
			log("[ShaderJIT] Unknown PICA opcode: %02X\n", opcode);
			emitPrintLog(shaderUnit);
			break;

		// We consider both MAD and MADI to be the same instruction and decode which one we actually have in recMAD
		case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37:
		case 0x38: case 0x39: case 0x3A: case 0x3B: case 0x3C: case 0x3D: case 0x3E: case 0x3F:
			recMAD(shaderUnit, instruction);
			break;

		case ShaderOpcodes::SLT:
		case ShaderOpcodes::SLTI:
			recSLT(shaderUnit, instruction); break;

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

		case 3: {
			const uintptr_t loopCounterOffset = uintptr_t(&shader.loopCounter) - uintptr_t(&shader);
			mov(eax, dword[statePointer + loopCounterOffset]); // rax = loop counter
			break;
		}
		
		default:
			Helpers::panic("[ShaderJIT]: Unimplemented source index type %d", index);
	}

	// Swizzle and load register into dest, from [state pointer + rcx + offset] and apply the relevant swizzle
	auto swizzleAndLoadReg = [this, &dest, &compSwizzle, &convertedSwizzle](size_t offset) {
		if (compSwizzle == noSwizzle)  // Avoid emitting swizzle if not necessary
			movaps(dest, xword[statePointer + rcx + offset]);
		else  // Swizzle is not trivial so we need to emit a shuffle instruction
			pshufd(dest, xword[statePointer + rcx + offset], convertedSwizzle);
	};

	// Here we handle what happens when using indexed addressing & we can't predict what register will be read at compile time
	// The index of the access is assumed to be in rax
	// Add source register (src) and index (rax) to form the final register
	add(rax, src);

	Label maybeTemp, maybeUniform, unknownReg, end;
	const uintptr_t inputOffset = uintptr_t(&shader.inputs[0]) - uintptr_t(&shader);
	const uintptr_t tempOffset = uintptr_t(&shader.tempRegisters[0]) - uintptr_t(&shader);
	const uintptr_t uniformOffset = uintptr_t(&shader.floatUniforms[0]) - uintptr_t(&shader);

	// If reg < 0x10, return inputRegisters[reg]
	cmp(rax, 0x10);
	jae(maybeTemp);
	mov(rcx, rax);
	shl(rcx, 4);  // rcx = rax * sizeof(vec4 of floats) = rax * 16
	swizzleAndLoadReg(inputOffset);
	jmp(end);
	
	// If (reg < 0x1F) return tempRegisters[reg - 0x10]
	L(maybeTemp);
	cmp(rax, 0x20);
	jae(maybeUniform);
	lea(rcx, qword[rax - 0x10]);
	shl(rcx, 4);
	swizzleAndLoadReg(tempOffset);
	jmp(end);

	// If (reg < 0x80) return floatUniforms[reg - 0x20]
	L(maybeUniform);
	cmp(rax, 0x80);
	jae(unknownReg);
	lea(rcx, qword[rax - 0x20]);
	shl(rcx, 4);
	swizzleAndLoadReg(uniformOffset);
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
			if (haveAVX) {
				vpsrldq(scratch1, source, index * sizeof(float));
			} else {
				movaps(scratch1, source);
				psrldq(scratch1, index * sizeof(float));
			}
			movss(dword[statePointer + lane_offset], scratch1);
		}
	} else if (haveSSE4_1) {
		// Bit reverse the write mask because that is what blendps expects
		u32 adjustedMask = ((writeMask >> 3) & 0b1) | ((writeMask >> 1) & 0b10) | ((writeMask << 1) & 0b100) | ((writeMask << 3) & 0b1000);
		// Don't accidentally overwrite scratch1 if that is what we're writing derp
		Xmm temp = (source == scratch1) ? scratch2 : scratch1;

		movaps(temp, xword[statePointer + offset]);     // Read current value of dest
		blendps(temp, source, adjustedMask);            // Blend with source
		movaps(xword[statePointer + offset], temp);     // Write back
	} else {
		// Blend algo referenced from Citra
		const u8 selector = (((writeMask & 0b1000) ? 1 : 0) << 0) |
			(((writeMask & 0b0100) ? 3 : 2) << 2) |
			(((writeMask & 0b0010) ? 0 : 1) << 4) |
			(((writeMask & 0b0001) ? 2 : 3) << 6);
		
		// Reorder instructions based on whether the source == scratch1. This is to avoid overwriting scratch1 if it's the source,
		// While also having the memory load come first to mitigate execution hazards and give the load more time to complete before reading if possible
		if (source != scratch1) {
			movaps(scratch1, xword[statePointer + offset]);
			movaps(scratch2, source);
		} else {
			movaps(scratch2, source);
			movaps(scratch1, xword[statePointer + offset]);
		}
		
		unpckhps(scratch2, scratch1); // Unpack X/Y components of source and destination
		unpcklps(scratch1, source);   // Unpack Z/W components of source and destination
		shufps(scratch1, scratch2, selector); // "merge-shuffle" dest and source using selecto
		movaps(xword[statePointer + offset], scratch1); // Write back
	}
}

void ShaderEmitter::checkCmpRegister(const PICAShader& shader, u32 instruction) {
	static_assert(sizeof(bool) == 1 && sizeof(shader.cmpRegister) == 2); // The code below relies on bool being 1 byte exactly
	const size_t cmpRegXOffset = uintptr_t(&shader.cmpRegister[0]) - uintptr_t(&shader);
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
	// Deallocate the 8 bytes taken up for the return guard + the 8 bytes of rsp padding we inserted in the prologue
	add(rsp, 16);
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

void ShaderEmitter::recFLR(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	const u32 src = getBits<12, 7>(instruction);
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	loadRegister<1>(src1_xmm, shader, src, idx, operandDescriptor); // Load source 1 into scratch1
	if (haveSSE4_1) {
		roundps(src1_xmm, src1_xmm, _MM_FROUND_FLOOR);
	} else {
		cvttps2dq(src1_xmm, src1_xmm); // Truncate and convert to integer
		cvtdq2ps(src1_xmm, src1_xmm);  // Convert from integer back to float
	}

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
		cvttss2si(eax, src1_xmm); // Convert bottom lane
		mov(dword[statePointer + addrRegisterOffset], eax); // Write it back
	}
	else if (writeY) {
		psrldq(src1_xmm, sizeof(float)); // Shift y component to bottom lane
		cvttss2si(eax, src1_xmm); // Convert bottom lane
		mov(dword[statePointer + addrRegisterYOffset], eax); // Write it back to y component
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

void ShaderEmitter::recDPH(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	const u32 src1 = getBits<12, 7>(instruction);
	const u32 src2 = getBits<7, 5>(instruction);  // src2 coming first because PICA moment
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	// TODO: Safe multiplication equivalent (Multiplication is not IEEE compliant on the PICA)
	loadRegister<1>(src1_xmm, shader, src1, idx, operandDescriptor);
	loadRegister<2>(src2_xmm, shader, src2, 0, operandDescriptor);

	// Attach 1.0 to the w component of src1
	if (haveSSE4_1) {
		blendps(src1_xmm, xword[rip + onesVector], 0b1000);
	} else {
		movaps(scratch1, src1_xmm);
		unpckhps(scratch1, xword[rip + onesVector]);
		unpcklpd(src1_xmm, scratch1);
	}

	dpps(src1_xmm, src2_xmm, 0b11111111);  // 4-lane dot product between the 2 registers, store the result in all lanes of scratch1 similarly to PICA
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

void ShaderEmitter::recMIN(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	const u32 src1 = getBits<12, 7>(instruction);
	const u32 src2 = getBits<7, 5>(instruction); // src2 coming first because PICA moment
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	loadRegister<1>(src1_xmm, shader, src1, idx, operandDescriptor);
	loadRegister<2>(src2_xmm, shader, src2, 0, operandDescriptor);
	minps(src1_xmm, src2_xmm);
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
	const bool isMADI = getBit<29>(instruction) == 0;

	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x1f];
	const u32 src1 = getBits<17, 5>(instruction);
	const u32 src2 = isMADI ? getBits<12, 5>(instruction) : getBits<10, 7>(instruction);
	const u32 src3 = isMADI ? getBits<5, 7>(instruction) : getBits<5, 5>(instruction);
	const u32 idx = getBits<22, 2>(instruction);
	const u32 dest = getBits<24, 5>(instruction);

	loadRegister<1>(src1_xmm, shader, src1, 0, operandDescriptor);
	loadRegister<2>(src2_xmm, shader, src2, isMADI ? 0 : idx, operandDescriptor);
	loadRegister<3>(src3_xmm, shader, src3, isMADI ? idx : 0, operandDescriptor);

	// TODO: Implement safe PICA mul
	// If we have FMA3, optimize MAD to use FMA
	if (haveFMA3) {
		vfmadd213ps(src1_xmm, src2_xmm, src3_xmm);
		storeRegister(src1_xmm, shader, dest, operandDescriptor);
	}
	
	// If we don't have FMA3, do a multiplication and addition
	else {
		// Multiply src1 * src2
		if (haveAVX) {
			vmulps(scratch1, src1_xmm, src2_xmm);
		} else {
			movaps(scratch1, src1_xmm);
			mulps(scratch1, src2_xmm);
		}

		// Add src3
		addps(scratch1, src3_xmm);
		storeRegister(scratch1, shader, dest, operandDescriptor);
	}
}

void ShaderEmitter::recSLT(const PICAShader& shader, u32 instruction) {
	const bool isSLTI = (instruction >> 26) == ShaderOpcodes::SLTI;
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];

	const u32 src1 = isSLTI ? getBits<14, 5>(instruction) : getBits<12, 7>(instruction);
	const u32 src2 = isSLTI ? getBits<7, 7>(instruction) : getBits<7, 5>(instruction);
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	loadRegister<1>(src1_xmm, shader, src1, isSLTI ? 0 : idx, operandDescriptor);
	loadRegister<2>(src2_xmm, shader, src2, isSLTI ? idx : 0, operandDescriptor);
	cmpltps(src1_xmm, src2_xmm);
	andps(src1_xmm, xword[rip + onesVector]);
	storeRegister(src1_xmm, shader, dest, operandDescriptor);
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

	static_assert(sizeof(shader.cmpRegister[0]) == 1 && sizeof(shader.cmpRegister) == 2); // The code below relies on bool being 1 byte exactly
	const size_t cmpRegXOffset = uintptr_t(&shader.cmpRegister[0]) - uintptr_t(&shader);
	const size_t cmpRegYOffset = cmpRegXOffset + sizeof(bool);

	// Cmp x and y are the same compare function, we can use a single cmp instruction
	if (cmpX == cmpY) {
		cmpps(lhs_x, rhs_x, compareFuncX);
		movq(rax, lhs_x); // Move both comparison results to rax
		test(eax, eax);   // Check bottom 32 bits first
		setne(byte[statePointer + cmpRegXOffset]); // set cmp.x

		shr(rax, 32);     // Check top 32 bits (shr will set the zero flag properly)
		setne(byte[statePointer + cmpRegYOffset]); // set cmp.y
	} else {
		if (haveAVX) {
			vcmpps(scratch1, lhs_x, rhs_x, compareFuncX); // Perform comparison for X component and store result in scratch1
			vcmpps(scratch2, lhs_y, rhs_y, compareFuncY); // Perform comparison for Y component and store result in scratch2
		} else {
			movaps(scratch1, lhs_x); // Copy the left hand operands to temp registers
			movaps(scratch2, lhs_y);

			cmpps(scratch1, rhs_x, compareFuncX); // Perform the compares
			cmpps(scratch2, rhs_y, compareFuncY);
		}

		movd(eax, scratch1); // Move results to eax for X and edx for Y
		movq(rdx, scratch2);

		test(eax, eax);      // Write back results with setne
		setne(byte[statePointer + cmpRegXOffset]);
		shr(rdx, 32);        // We want the y component for the second comparison. This shift will set zero flag to 0 if the comparison is true
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
		jmp(endIf, T_NEAR); // Skip executing the else branch if the if branch was ran
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
	} else { // Else block is NOT empty
		jmp(endIf, T_NEAR); // Skip executing the else branch if the if branch was ran
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
	// Call subroutine, Xbyak will update the label if it hasn't been initialized yet
	call(instructionLabels[dest]);

	// Fix up stack after returning by adding 8 to rsp and "popping" the return PC
	add(rsp, 8);
}

void ShaderEmitter::recCALLC(const PICAShader& shader, u32 instruction) {
	Label skipCall;

	// z is 1 if the call should be taken, 0 otherwise
	checkCmpRegister(shader, instruction);
	jnz(skipCall);
	recCALL(shader, instruction);

	L(skipCall);
}

void ShaderEmitter::recCALLU(const PICAShader& shader, u32 instruction) {
	Label skipCall;

	// z is 0 if the call should be taken, 1 otherwise
	checkBoolUniform(shader, instruction);
	jz(skipCall);
	recCALL(shader, instruction);

	L(skipCall);
}

void ShaderEmitter::recJMPC(const PICAShader& shader, u32 instruction) {
	const u32 dest = getBits<10, 12>(instruction);

	Label& l = instructionLabels[dest];
	// Z is 1 if the comparison is true
	checkCmpRegister(shader, instruction);
	jz(l, T_NEAR);
}

void ShaderEmitter::recJMPU(const PICAShader& shader, u32 instruction) {
	bool jumpIfFalse = instruction & 1; // If the LSB is 0 we want to compare to true, otherwise compare to false
	const u32 dest = getBits<10, 12>(instruction);

	Label& l = instructionLabels[dest];
	// Z is 0 if the uniform is true
	checkBoolUniform(shader, instruction);

	if (jumpIfFalse) {
		jz(l, T_NEAR);
	} else {
		jnz(l, T_NEAR);
	}
}

void ShaderEmitter::recLOOP(const PICAShader& shader, u32 instruction) {
	const u32 dest = getBits<10, 12>(instruction);
	const u32 uniformIndex = getBits<22, 2>(instruction);

	if (loopLevel > 0) {
		log("[Shader JIT] Detected nested loop. Might be broken?\n");
	}

	if (dest < recompilerPC) {
		Helpers::panic("[Shader JIT] Detected backwards loop\n");
	}

	loopLevel++;

	// Offset of the uniform
	const auto& uniform = shader.intUniforms[uniformIndex];
	const uintptr_t uniformOffset = uintptr_t(&uniform[0]) - uintptr_t(&shader);
	// Offset of the loop register
	const uintptr_t loopRegOffset = uintptr_t(&shader.loopCounter) - uintptr_t(&shader);

	movzx(eax, byte[statePointer + uniformOffset]); // eax = loop iteration count
	movzx(ecx, byte[statePointer + uniformOffset + sizeof(u8)]); // ecx = initial loop counter value
	movzx(edx, byte[statePointer + uniformOffset + 2 * sizeof(u8)]); // edx = loop increment

	add(eax, 1); // The iteration count is actually uniform.x + 1
	mov(dword[statePointer + loopRegOffset], ecx); // Set loop counter
	
	// TODO: This might break if an instruction in a loop decides to yield...
	push(rax);  // Push loop iteration counter
	push(rdx);  // Push loop increment

	Label loopStart;
	L(loopStart);
	compileUntil(shader, dest + 1);

	const size_t stackOffsetOfLoopIncrement = 0;
	const size_t stackOffsetOfIterationCounter = stackOffsetOfLoopIncrement + 8;

	mov(ecx, dword[rsp + stackOffsetOfLoopIncrement]);   // ecx = Loop increment
	add(dword[statePointer + loopRegOffset], ecx);       // Increment loop counter
	sub(dword[rsp + stackOffsetOfIterationCounter], 1);  // Subtract 1 from loop iteration counter

	jnz(loopStart);  // Back to loop start if not over
	add(rsp, 16);
	loopLevel--;
}

void ShaderEmitter::recLG2(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	const u32 src = getBits<12, 7>(instruction);
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);
	const u32 writeMask = getBits<0, 4>(operandDescriptor);

	loadRegister<1>(src1_xmm, shader, src, idx, operandDescriptor);
	call(log2Func); // Result is output in src1_xmm
	
	if (writeMask != 0x8) {             // Copy bottom lane to all lanes if we're not simply writing back x
		shufps(src1_xmm, src1_xmm, 0);  // src1_xmm = src1_xmm.xxxx
	}

	storeRegister(src1_xmm, shader, dest, operandDescriptor);
}

void ShaderEmitter::recEX2(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	const u32 src = getBits<12, 7>(instruction);
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);
	const u32 writeMask = getBits<0, 4>(operandDescriptor);

	loadRegister<1>(src1_xmm, shader, src, idx, operandDescriptor);
	call(exp2Func);  // Result is output in src1_xmm

	if (writeMask != 0x8) {             // Copy bottom lane to all lanes if we're not simply writing back x
		shufps(src1_xmm, src1_xmm, 0);  // src1_xmm = src1_xmm.xxxx
	}

	storeRegister(src1_xmm, shader, dest, operandDescriptor);
}

void ShaderEmitter::printLog(const PICAShader& shaderUnit) {
	printf("PC: %04X\n", shaderUnit.pc);

	for (int i = 0; i < shaderUnit.tempRegisters.size(); i++) {
		const auto& r = shaderUnit.tempRegisters[i];
		printf("t%d: (%.2f, %.2f, %.2f, %.2f)\n", i, r[0].toFloat64(), r[1].toFloat64(), r[2].toFloat64(), r[3].toFloat64());
	}

	for (int i = 0; i < shaderUnit.outputs.size(); i++) {
		const auto& r = shaderUnit.outputs[i];
		printf("o%d: (%.2f, %.2f, %.2f, %.2f)\n", i, r[0].toFloat64(), r[1].toFloat64(), r[2].toFloat64(), r[3].toFloat64());
	}

	printf("addr: (%d, %d)\n", shaderUnit.addrRegister[0], shaderUnit.addrRegister[1]);
	printf("cmp: (%d, %d)\n", shaderUnit.cmpRegister[0], shaderUnit.cmpRegister[1]);
}

// For EXP2/LOG2, we have permission to adjust and relicense the SSE implementation from Citra for this project from the original authors
// So we do it since EXP2/LOG2 are pretty terrible to implement.
// ABI: Input is in the bottom bits of src1_xmm, same for output. If the result needs swizzling, the caller must handle it
// Assume src1, src2, scratch1, scratch2, eax, edx all thrashed

Xbyak::Label ShaderEmitter::emitLog2Func() {
	Xbyak::Label subroutine;

	// This code uses the fact that log2(float) = log2(2^exponent * mantissa)
	// = log2(2^exponent) + log2(mantissa) = exponent + log2(mantissa) where mantissa has a limited range of values
	// https://stackoverflow.com/a/45787548

	// SSE does not have a log instruction, thus we must approximate.
	// We perform this approximation first performing a range reduction into the range [1.0, 2.0).
	// A minimax polynomial which was fit for the function log2(x) / (x - 1) is then evaluated.
	// We multiply the result by (x - 1) then restore the result into the appropriate range.

	// Coefficients for the minimax polynomial.
	// f(x) computes approximately log2(x) / (x - 1).
	// f(x) = c4 + x * (c3 + x * (c2 + x * (c1 + x * c0)).
	// We align the table of coefficients to 64 bytes, so that the whole thing will fit in 1 cache line
	align(64);
	const void* c0 = getCurr();
	dd(0x3d74552f);
	const void* c1 = getCurr();
	dd(0xbeee7397);
	const void* c2 = getCurr();
	dd(0x3fbd96dd);
	const void* c3 = getCurr();
	dd(0xc02153f6);
	const void* c4 = getCurr();
	dd(0x4038d96c);

	align(16);
	const void* negative_infinity_vector = getCurr();
	dd(0xff800000);
	dd(0xff800000);
	dd(0xff800000);
	dd(0xff800000);
	const void* default_qnan_vector = getCurr();
	dd(0x7fc00000);
	dd(0x7fc00000);
	dd(0x7fc00000);
	dd(0x7fc00000);

	Xbyak::Label inputIsNan, inputIsZero, inputOutOfRange;

	align(16);
	L(inputOutOfRange);
	je(inputIsZero);
	movaps(src1_xmm, xword[rip + default_qnan_vector]);
	ret();
	L(inputIsZero);
	movaps(src1_xmm, xword[rip + negative_infinity_vector]);
	ret();

	align(16);
	L(subroutine);

	// Here we handle edge cases: input in {NaN, 0, -Inf, Negative}.
	xorps(scratch1, scratch1);
	ucomiss(scratch1, src1_xmm);
	jp(inputIsNan);
	jae(inputOutOfRange);

	// Split input: SRC1=MANT[1,2) SCRATCH2=Exponent
	if (cpuCaps.has(Cpu::tAVX512F | Cpu::tAVX512VL)) {
		vgetexpss(scratch2, src1_xmm, src1_xmm);
		vgetmantss(src1_xmm, src1_xmm, src1_xmm, 0);
	} else {
		movd(eax, src1_xmm);
		mov(edx, eax);
		and_(eax, 0x7f800000);
		and_(edx, 0x007fffff);
		or_(edx, 0x3f800000);
		movd(src1_xmm, edx);
		// SRC1 now contains the mantissa of the input.
		shr(eax, 23);
		sub(eax, 0x7f);
		cvtsi2ss(scratch2, eax);
		// scratch2 now contains the exponent of the input.
	}

	movss(scratch1, xword[rip + c0]);

	// Complete computation of polynomial
	if (haveFMA3) {
		vfmadd213ss(scratch1, src1_xmm, xword[rip + c1]);
		vfmadd213ss(scratch1, src1_xmm, xword[rip + c2]);
		vfmadd213ss(scratch1, src1_xmm, xword[rip + c3]);
		vfmadd213ss(scratch1, src1_xmm, xword[rip + c4]);
		subss(src1_xmm, dword[rip + onesVector]);
		vfmadd231ss(scratch2, scratch1, src1_xmm);
	} else {
		mulss(scratch1, src1_xmm);
		addss(scratch1, xword[rip + c1]);
		mulss(scratch1, src1_xmm);
		addss(scratch1, xword[rip + c2]);
		mulss(scratch1, src1_xmm);
		addss(scratch1, xword[rip + c3]);
		mulss(scratch1, src1_xmm);
		subss(src1_xmm, dword[rip + onesVector]);
		addss(scratch1, xword[rip + c4]);
		mulss(scratch1, src1_xmm);
		addss(scratch2, scratch1);
	}

	xorps(src1_xmm, src1_xmm);  // break dependency chain
	movss(src1_xmm, scratch2);
	L(inputIsNan);

	ret();
	return subroutine;
}

Xbyak::Label ShaderEmitter::emitExp2Func() {
	Xbyak::Label subroutine;

	// SSE does not have a exp instruction, thus we must approximate.
	// We perform this approximation first performaing a range reduction into the range [-0.5, 0.5).
	// A minimax polynomial which was fit for the function exp2(x) is then evaluated.
	// We then restore the result into the appropriate range.

	// Similarly to log2, we align our literal pool to 64 bytes to make sure the whole thing fits in 1 cache line
	align(64);
	const void* input_max = getCurr();
	dd(0x43010000);
	const void* input_min = getCurr();
	dd(0xc2fdffff);
	const void* c0 = getCurr();
	dd(0x3c5dbe69);
	const void* half = getCurr();
	dd(0x3f000000);
	const void* c1 = getCurr();
	dd(0x3d5509f9);
	const void* c2 = getCurr();
	dd(0x3e773cc5);
	const void* c3 = getCurr();
	dd(0x3f3168b3);
	const void* c4 = getCurr();
	dd(0x3f800016);

	Xbyak::Label retLabel;

	align(16);
	L(subroutine);

	// Handle edge cases
	ucomiss(src1_xmm, src1_xmm);
	jp(retLabel);

	// Decompose input:
	// SCRATCH=2^round(input)
	// SRC1=input-round(input) [-0.5, 0.5)
	if (cpuCaps.has(Cpu::tAVX512F | Cpu::tAVX512VL)) {
		// Cheat a bit and store ones in src2 since the register is unused
		vmovaps(src2_xmm, xword[rip + onesVector]);
		// input - 0.5
		vsubss(scratch1, src1_xmm, xword[rip + half]);

		// trunc(input - 0.5)
		vrndscaless(scratch2, scratch1, scratch1, _MM_FROUND_TRUNC);

		// SCRATCH = 1 * 2^(trunc(input - 0.5))
		vscalefss(scratch1, src2_xmm, scratch2);

		// SRC1 = input-trunc(input - 0.5)
		vsubss(src1_xmm, src1_xmm, scratch2);
	} else {
		// Clamp to maximum range since we shift the value directly into the exponent.
		minss(src1_xmm, xword[rip + input_max]);
		maxss(src1_xmm, xword[rip + input_min]);

		if (cpuCaps.has(Cpu::tAVX)) {
			vsubss(scratch1, src1_xmm, xword[rip + half]);
		} else {
			movss(scratch1, src1_xmm);
			subss(scratch1, xword[rip + half]);
		}

		if (cpuCaps.has(Cpu::tSSE41)) {
			roundss(scratch1, scratch1, _MM_FROUND_TRUNC);
			cvtss2si(eax, scratch1);
		} else {
			cvtss2si(eax, scratch1);
			cvtsi2ss(scratch1, eax);
		}
		// SCRATCH now contains input rounded to the nearest integer.
		add(eax, 0x7f);
		subss(src1_xmm, scratch1);
		// SRC1 contains input - round(input), which is in [-0.5, 0.5).
		shl(eax, 23);
		movd(scratch1, eax);
		// SCRATCH contains 2^(round(input)).
	}

	// Complete computation of polynomial.
	movss(scratch2, xword[rip + c0]);

	if (haveFMA3) {
		vfmadd213ss(scratch2, src1_xmm, xword[rip + c1]);
		vfmadd213ss(scratch2, src1_xmm, xword[rip + c2]);
		vfmadd213ss(scratch2, src1_xmm, xword[rip + c3]);
		vfmadd213ss(src1_xmm, scratch2, xword[rip + c4]);
	} else {
		mulss(scratch2, src1_xmm);
		addss(scratch2, xword[rip + c1]);
		mulss(scratch2, src1_xmm);
		addss(scratch2, xword[rip + c2]);
		mulss(scratch2, src1_xmm);
		addss(scratch2, xword[rip + c3]);
		mulss(src1_xmm, scratch2);
		addss(src1_xmm, xword[rip + c4]);
	}

	mulss(src1_xmm, scratch1);
	L(retLabel);

	ret();
	return subroutine;
}

// As we mentioned above, this function is uber slow because we don't expect the shader JIT to call HLL functions in real scenarios
// Aside from debugging code. So we don't care for this function to be performant or anything of the like.  It is quick and dirty
// And mostly meant to be used for generating logs to diff the JIT and interpreter
// We also don't support stack arguments atm unless it becomes actually necessary
void ShaderEmitter::emitPrintLog(const PICAShader& shaderUnit) {
	const uintptr_t pcOffset = uintptr_t(&shaderUnit.pc) - uintptr_t(&shaderUnit);
	// Write back PC to print it
	mov(dword[statePointer + pcOffset], recompilerPC);

	// Push all registers because our JIT assumes everything is non volatile
	push(rbp);
	push(rax);
	push(rbx);
	push(rcx);
	push(rdx);
	push(rsi);
	push(rdi);
	push(r8);
	push(r9);
	push(r10);
	push(r11);
	push(r12);
	push(r13);
	push(r14);
	push(r15);

	mov(rbp, rsp);
	// Reserve a bunch of stack space for Windows shadow stack et al, then force align rsp to 16 bytes to respect the ABI
	sub(rsp, 64);
	and_(rsp, ~0xF);

	// Call function
	mov(arg1, statePointer);
	mov(rax, uintptr_t(printLog));
	call(rax);

	// Undo anything we did
	mov(rsp, rbp);
	pop(r15);
	pop(r14);
	pop(r13);
	pop(r12);
	pop(r11);
	pop(r10);
	pop(r9);
	pop(r8);
	pop(rdi);
	pop(rsi);
	pop(rdx);
	pop(rcx);
	pop(rbx);
	pop(rax);
	pop(rbp);
}

#endif