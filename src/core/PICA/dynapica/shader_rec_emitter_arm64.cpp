#if defined(PANDA3DS_DYNAPICA_SUPPORTED) && defined(PANDA3DS_ARM64_HOST)
#include "PICA/dynapica/shader_rec_emitter_arm64.hpp"

#include <bit>

using namespace Helpers;
using namespace oaknut;
using namespace oaknut::util;

// TODO: Expose safe/unsafe optimizations to the user
constexpr bool useSafeMUL = true;

// Similar to the x64 recompiler, we use an odd internal ABI, which abuses the fact that we'll very rarely be calling C++ functions
// So to avoid pushing and popping, we'll be making use of volatile registers as much as possible
static constexpr QReg scratch1 = Q0;
static constexpr QReg scratch2 = Q1;
static constexpr QReg src1_vec = Q2;
static constexpr QReg src2_vec = Q3;
static constexpr QReg src3_vec = Q4;
static constexpr QReg onesVector = Q5;

static constexpr XReg arg1 = X0;
static constexpr XReg arg2 = X1;
static constexpr XReg statePointer = X9;

void ShaderEmitter::compile(const PICAShader& shaderUnit) {
	oaknut::CodeBlock::unprotect();  // Unprotect the memory before writing to it

	// Constants
	align(16);
	// Generate blending masks for doing masked writes to registers
	l(blendMasks);
	for (int i = 0; i < 16; i++) {
		dw((i & 0x8) ? 0xFFFFFFFF : 0);  // Mask for x component
		dw((i & 0x4) ? 0xFFFFFFFF : 0);  // Mask for y component
		dw((i & 0x2) ? 0xFFFFFFFF : 0);  // Mask for z component
		dw((i & 0x1) ? 0xFFFFFFFF : 0);  // Mask for w component
	}

	// Emit prologue first
	oaknut::Label prologueLabel;
	align(16);

	l(prologueLabel);
	prologueCb = getLabelPointer<PrologueCallback>(prologueLabel);

	// Set state pointer to the proper pointer
	// state pointer is volatile, no need to preserve it
	MOV(statePointer, arg1);
	// Generate a vector of all 1.0s for SLT/SGE/RCP/RSQ
	FMOV(onesVector.S4(), FImm8(0x70));

	// Push a return guard on the stack. This happens due to the way we handle the PICA callstack, by pushing the return PC to stack
	// By pushing -1, we make it impossible for a return check to erroneously pass
	MOV(arg1, 0xffffffffffffffffll);
	// Backup link register (X30) and push return guard
	STP(arg1, X30, SP, PRE_INDEXED, -16);

	// Jump to code with a tail call
	BR(arg2);

	// Scan the code for call, exp2, log2, etc instructions which need some special care
	// After that, emit exp2 and log2 functions if the corresponding instructions are present
	scanCode(shaderUnit);
	if (codeHasExp2) Helpers::panic("arm64 shader JIT: Code has exp2");
	if (codeHasLog2) Helpers::panic("arm64 shader JIT: Code has log2");

	align(16);
	// Compile every instruction in the shader
	// This sounds horrible but the PICA instruction memory is tiny, and most of the time it's padded wtih nops that compile to nothing
	recompilerPC = 0;
	loopLevel = 0;
	compileUntil(shaderUnit, PICAShader::maxInstructionCount);

	// Protect the memory and invalidate icache before executing the code
	oaknut::CodeBlock::protect();
	oaknut::CodeBlock::invalidate_all();
}

void ShaderEmitter::scanCode(const PICAShader& shaderUnit) {
	returnPCs.clear();

	for (u32 i = 0; i < PICAShader::maxInstructionCount; i++) {
		const u32 instruction = shaderUnit.loadedShader[i];
		const u32 opcode = instruction >> 26;

		if (isCall(instruction)) {
			const u32 num = instruction & 0xff;
			const u32 dest = getBits<10, 12>(instruction);
			const u32 returnPC = num + dest;  // Add them to get the return PC

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
	l(instructionLabels[recompilerPC]);

	// See if PC is a possible return PC and emit the proper code if so
	if (std::binary_search(returnPCs.begin(), returnPCs.end(), recompilerPC)) {
		Label skipReturn;

		LDP(X0, XZR, SP);       // W0 = Next return address
		MOV(W1, recompilerPC);  // W1 = Current PC
		CMP(W0, W1);            // If they're equal, execute a RET, otherwise skip it
		B(NE, skipReturn);
		RET();

		l(skipReturn);
	}

	// Fetch instruction and inc PC
	const u32 instruction = shaderUnit.loadedShader[recompilerPC++];
	const u32 opcode = instruction >> 26;

	switch (opcode) {
		case ShaderOpcodes::ADD: recADD(shaderUnit, instruction); break;
		case ShaderOpcodes::CALL: recCALL(shaderUnit, instruction); break;
		case ShaderOpcodes::CALLC: recCALLC(shaderUnit, instruction); break;
		case ShaderOpcodes::CALLU: recCALLU(shaderUnit, instruction); break;
		case ShaderOpcodes::CMP1:
		case ShaderOpcodes::CMP2: recCMP(shaderUnit, instruction); break;
		case ShaderOpcodes::DP3: recDP3(shaderUnit, instruction); break;
		case ShaderOpcodes::DP4: recDP4(shaderUnit, instruction); break;
		// case ShaderOpcodes::DPH:
		// case ShaderOpcodes::DPHI: recDPH(shaderUnit, instruction); break;
		case ShaderOpcodes::END: recEND(shaderUnit, instruction); break;
		// case ShaderOpcodes::EX2: recEX2(shaderUnit, instruction); break;
		case ShaderOpcodes::FLR: recFLR(shaderUnit, instruction); break;
		case ShaderOpcodes::IFC: recIFC(shaderUnit, instruction); break;
		case ShaderOpcodes::IFU: recIFU(shaderUnit, instruction); break;
		case ShaderOpcodes::JMPC: recJMPC(shaderUnit, instruction); break;
		case ShaderOpcodes::JMPU: recJMPU(shaderUnit, instruction); break;
		// case ShaderOpcodes::LG2: recLG2(shaderUnit, instruction); break;
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
		case ShaderOpcodes::SETEMIT: log("[ShaderJIT] Unimplemented PICA opcode: %02X\n", opcode); break;

		case ShaderOpcodes::BREAK:
		case ShaderOpcodes::BREAKC: Helpers::warn("[Shader JIT] Unimplemented BREAK(C) instruction!"); break;

		// We consider both MAD and MADI to be the same instruction and decode which one we actually have in recMAD
		case 0x30:
		case 0x31:
		case 0x32:
		case 0x33:
		case 0x34:
		case 0x35:
		case 0x36:
		case 0x37:
		case 0x38:
		case 0x39:
		case 0x3A:
		case 0x3B:
		case 0x3C:
		case 0x3D:
		case 0x3E:
		case 0x3F: recMAD(shaderUnit, instruction); break;

		case ShaderOpcodes::SLT:
		case ShaderOpcodes::SLTI: recSLT(shaderUnit, instruction); break;

		case ShaderOpcodes::SGE:
		case ShaderOpcodes::SGEI: recSGE(shaderUnit, instruction); break;

		default: Helpers::panic("Shader JIT: Unimplemented PICA opcode %X", opcode);
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
void ShaderEmitter::loadRegister(QReg dest, const PICAShader& shader, u32 src, u32 index, u32 operandDescriptor) {
	u32 compSwizzle;  // Component swizzle pattern for the register
	bool negate;      // If true, negate all lanes of the register

	if constexpr (sourceIndex == 1) {  // SRC1
		negate = (getBit<4>(operandDescriptor)) != 0;
		compSwizzle = getBits<5, 8>(operandDescriptor);
	} else if constexpr (sourceIndex == 2) {  // SRC2
		negate = (getBit<13>(operandDescriptor)) != 0;
		compSwizzle = getBits<14, 8>(operandDescriptor);
	} else if constexpr (sourceIndex == 3) {  // SRC3
		negate = (getBit<22>(operandDescriptor)) != 0;
		compSwizzle = getBits<23, 8>(operandDescriptor);
	}

	// TODO: Do indexes get applied if src < 0x20?

	switch (index) {
		case 0:
			[[likely]] {  // Keep src as is, no need to offset it
				const vec4f& srcRef = getSourceRef(shader, src);
				const uintptr_t offset = uintptr_t(&srcRef) - uintptr_t(&shader);  // Calculate offset of register from start of the state struct

				LDR(dest, statePointer, offset);
				switch (compSwizzle) {
					case noSwizzle: break;                              // .xyzw
					case 0x0: DUP(dest.S4(), dest.Selem()[0]); break;   // .xxxx
					case 0x55: DUP(dest.S4(), dest.Selem()[1]); break;  // .yyyy
					case 0xAA: DUP(dest.S4(), dest.Selem()[2]); break;  // .zzzz
					case 0xFF:
						DUP(dest.S4(), dest.Selem()[3]);
						break;  // .wwww

					// Some of these cases may still be optimizable
					default: {
						MOV(scratch1.B16(), dest.B16());  // Make a copy of the register

						const auto newX = getBits<6, 2>(compSwizzle);
						const auto newY = getBits<4, 2>(compSwizzle);
						const auto newZ = getBits<2, 2>(compSwizzle);
						const auto newW = getBits<0, 2>(compSwizzle);

						// If the lane swizzled into the new x component is NOT the current x component, swizzle the correct lane with a mov
						// Repeat for each component of the vector
						if (newX != 0) {
							MOV(dest.Selem()[0], scratch1.Selem()[newX]);
						}

						if (newY != 1) {
							MOV(dest.Selem()[1], scratch1.Selem()[newY]);
						}

						if (newZ != 2) {
							MOV(dest.Selem()[2], scratch1.Selem()[newZ]);
						}

						if (newW != 3) {
							MOV(dest.Selem()[3], scratch1.Selem()[newW]);
						}

						break;
					}
				}

				// Negate the register if necessary
				if (negate) {
					FNEG(dest.S4(), dest.S4());
				}
				return;  // Return. Rest of the function handles indexing which is not used if index == 0
			}

		case 1: {
			const uintptr_t addrXOffset = uintptr_t(&shader.addrRegister[0]) - uintptr_t(&shader);
			LDRSW(X0, statePointer, addrXOffset);  // X0 = address register X
			break;
		}

		case 2: {
			const uintptr_t addrYOffset = uintptr_t(&shader.addrRegister[1]) - uintptr_t(&shader);
			LDRSW(X0, statePointer, addrYOffset);  // X0 = address register Y
			break;
		}

		case 3: {
			const uintptr_t loopCounterOffset = uintptr_t(&shader.loopCounter) - uintptr_t(&shader);
			LDR(W0, statePointer, loopCounterOffset);  // X0 = loop counter
			break;
		}

		default: Helpers::panic("[ShaderJIT]: Unimplemented source index type %d", index);
	}

	// Swizzle and load register into dest, from [state pointer + X1 + offset] and apply the relevant swizzle. Thrashes X2
	auto swizzleAndLoadReg = [this, &dest, &compSwizzle](size_t offset) {
		MOV(X2, offset);
		ADD(X1, X1, X2);
		LDR(dest, statePointer, X1);

		switch (compSwizzle) {
			case noSwizzle: break;                              // .xyzw
			case 0x0: DUP(dest.S4(), dest.Selem()[0]); break;   // .xxxx
			case 0x55: DUP(dest.S4(), dest.Selem()[1]); break;  // .yyyy
			case 0xAA: DUP(dest.S4(), dest.Selem()[2]); break;  // .zzzz
			case 0xFF:
				DUP(dest.S4(), dest.Selem()[3]);
				break;  // .wwww

			// Some of these cases may still be optimizable
			default: {
				MOV(scratch1.B16(), dest.B16());  // Make a copy of the register

				const auto newX = getBits<6, 2>(compSwizzle);
				const auto newY = getBits<4, 2>(compSwizzle);
				const auto newZ = getBits<2, 2>(compSwizzle);
				const auto newW = getBits<0, 2>(compSwizzle);

				// If the lane swizzled into the new x component is NOT the current x component, swizzle the correct lane with a mov
				// Repeat for each component of the vector
				if (newX != 0) {
					MOV(dest.Selem()[0], scratch1.Selem()[newX]);
				}

				if (newY != 1) {
					MOV(dest.Selem()[1], scratch1.Selem()[newY]);
				}

				if (newZ != 2) {
					MOV(dest.Selem()[2], scratch1.Selem()[newZ]);
				}

				if (newW != 3) {
					MOV(dest.Selem()[3], scratch1.Selem()[newW]);
				}

				break;
			}
		}
	};

	// Here we handle what happens when using indexed addressing & we can't predict what register will be read at compile time
	// The index of the access is assumed to be in X0
	// Add source register (src) and index (X0) to form the final register
	ADD(X0, X0, src);

	Label maybeTemp, maybeUniform, unknownReg, end;
	const uintptr_t inputOffset = uintptr_t(&shader.inputs[0]) - uintptr_t(&shader);
	const uintptr_t tempOffset = uintptr_t(&shader.tempRegisters[0]) - uintptr_t(&shader);
	const uintptr_t uniformOffset = uintptr_t(&shader.floatUniforms[0]) - uintptr_t(&shader);

	// If reg < 0x10, return inputRegisters[reg]
	CMP(X0, 0x10);
	B(HS, maybeTemp);
	LSL(X1, X0, 4);
	swizzleAndLoadReg(inputOffset);
	B(end);

	// If (reg < 0x1F) return tempRegisters[reg - 0x10]
	l(maybeTemp);
	CMP(X0, 0x20);
	B(HS, maybeUniform);
	SUB(X1, X0, 0x10);
	LSL(X1, X1, 4);
	swizzleAndLoadReg(tempOffset);
	B(end);

	// If (reg < 0x80) return floatUniforms[reg - 0x20]
	l(maybeUniform);
	CMP(X0, 0x80);
	B(HS, unknownReg);
	SUB(X1, X0, 0x20);
	LSL(X1, X1, 4);
	swizzleAndLoadReg(uniformOffset);
	B(end);

	l(unknownReg);
	MOVI(dest.S4(), 0);  // Set dest to 0 if we're reading from a garbage register

	l(end);
	// Negate the register if necessary
	if (negate) {
		FNEG(dest.S4(), dest.S4());
	}
}

void ShaderEmitter::storeRegister(QReg source, const PICAShader& shader, u32 dest, u32 operandDescriptor) {
	const vec4f& destRef = getDestRef(shader, dest);
	const uintptr_t offset = uintptr_t(&destRef) - uintptr_t(&shader);  // Calculate offset of register from start of the state struct

	// Mask of which lanes to write
	u32 writeMask = operandDescriptor & 0xf;
	if (writeMask == 0xf) {  // No lanes are masked, just use STR
		STR(source, statePointer, offset);
	} else {
		u8* blendMaskPointer = getLabelPointer<u8*>(blendMasks);
		LDR(scratch1, statePointer, offset);               // Load current value
		LDR(scratch2, blendMaskPointer + writeMask * 16);  // Load write mask for blending

		BSL(scratch2.B16(), source.B16(), scratch1.B16());  // Scratch2 = (Source & mask) | (original & ~mask)
		STR(scratch2, statePointer, offset);                // Write it back
	}
}

void ShaderEmitter::recMOV(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	const u32 src = getBits<12, 7>(instruction);
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	loadRegister<1>(src1_vec, shader, src, idx, operandDescriptor);  // Load source 1 into scratch1
	storeRegister(src1_vec, shader, dest, operandDescriptor);
}

void ShaderEmitter::recFLR(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	const u32 src = getBits<12, 7>(instruction);
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	loadRegister<1>(src1_vec, shader, src, idx, operandDescriptor);  // Load source 1 into scratch1
	FRINTM(src1_vec.S4(), src1_vec.S4());                            // Floor it and store into dest
	storeRegister(src1_vec, shader, dest, operandDescriptor);
}

void ShaderEmitter::recMOVA(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	const u32 src = getBits<12, 7>(instruction);
	const u32 idx = getBits<19, 2>(instruction);

	const bool writeX = getBit<3>(operandDescriptor);  // Should we write the x component of the address register?
	const bool writeY = getBit<2>(operandDescriptor);

	static_assert(sizeof(shader.addrRegister) == 2 * sizeof(s32));  // Assert that the address register is 2 s32s
	const uintptr_t addrRegisterOffset = uintptr_t(&shader.addrRegister[0]) - uintptr_t(&shader);
	const uintptr_t addrRegisterYOffset = addrRegisterOffset + sizeof(shader.addrRegister[0]);

	// If no register is being written to then it is a nop. Probably not common but whatever
	if (!writeX && !writeY) return;

	loadRegister<1>(src1_vec, shader, src, idx, operandDescriptor);
	FCVTZS(src1_vec.S4(), src1_vec.S4());  // Convert src1 from floats to s32s with truncation

	// Write both together
	if (writeX && writeY) {
		STR(src1_vec.toD(), statePointer, addrRegisterOffset);
	} else if (writeX) {
		STR(src1_vec.toS(), statePointer, addrRegisterOffset);
	} else if (writeY) {
		MOV(W0, src1_vec.Selem()[1]);  // W0 = Y component
		STR(W0, statePointer, addrRegisterYOffset);
	}
}

void ShaderEmitter::recDP3(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	const u32 src1 = getBits<12, 7>(instruction);
	const u32 src2 = getBits<7, 5>(instruction);
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);
	const u32 writeMask = getBits<0, 4>(operandDescriptor);

	loadRegister<1>(src1_vec, shader, src1, idx, operandDescriptor);
	loadRegister<2>(src2_vec, shader, src2, 0, operandDescriptor);
	// Set W component of src1 to 0.0, so that the w factor of the following dp4 will become 0, making it equivalent to a dp3
	INS(src1_vec.Selem()[3], WZR);

	// Now do a full DP4
	// Do a piecewise multiplication of the vectors first
	if constexpr (useSafeMUL) {
		emitSafeMUL(src1_vec, src2_vec, scratch1);
	} else {
		FMUL(src1_vec.S4(), src1_vec.S4(), src2_vec.S4());
	}
	FADDP(src1_vec.S4(), src1_vec.S4(), src1_vec.S4());  // Now add the adjacent components together
	FADDP(src1_vec.toS(), src1_vec.toD().S2());          // Again for the bottom 2 lanes. Now the bottom lane contains the dot product

	if (writeMask != 0x8) {                       // Copy bottom lane to all lanes if we're not simply writing back x
		DUP(src1_vec.S4(), src1_vec.Selem()[0]);  // src1_vec = src1_vec.xxxx
	}

	storeRegister(src1_vec, shader, dest, operandDescriptor);
}

void ShaderEmitter::recDP4(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	const u32 src1 = getBits<12, 7>(instruction);
	const u32 src2 = getBits<7, 5>(instruction);
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);
	const u32 writeMask = getBits<0, 4>(operandDescriptor);

	loadRegister<1>(src1_vec, shader, src1, idx, operandDescriptor);
	loadRegister<2>(src2_vec, shader, src2, 0, operandDescriptor);

	// Do a piecewise multiplication of the vectors first
	if constexpr (useSafeMUL) {
		emitSafeMUL(src1_vec, src2_vec, scratch1);
	} else {
		FMUL(src1_vec.S4(), src1_vec.S4(), src2_vec.S4());
	}
	FADDP(src1_vec.S4(), src1_vec.S4(), src1_vec.S4());  // Now add the adjacent components together
	FADDP(src1_vec.toS(), src1_vec.toD().S2());          // Again for the bottom 2 lanes. Now the bottom lane contains the dot product

	if (writeMask != 0x8) {                       // Copy bottom lane to all lanes if we're not simply writing back x
		DUP(src1_vec.S4(), src1_vec.Selem()[0]);  // src1_vec = src1_vec.xxxx
	}

	storeRegister(src1_vec, shader, dest, operandDescriptor);
}

void ShaderEmitter::emitSafeMUL(oaknut::QReg src1, oaknut::QReg src2, oaknut::QReg scratch0) {
	// 0 * inf and inf * 0 in the PICA should return 0 instead of NaN
	// This can be done by checking for NaNs before and after a multiplication

	// FMULX returns 2.0 in the case of 0.0 * inf or inf * 0.0
	// Both a FMUL and FMULX are done and the results are compared to each other
	// In the case that the results are diferent(a 0.0*inf happened), then
	// a 0.0 is written
	FMULX(scratch1.S4(), src1.S4(), src2.S4());
	FMUL(src1.S4(), src1.S4(), src2.S4());
	CMEQ(scratch1.S4(), scratch1.S4(), src1.S4());
	AND(src1.B16(), src1.B16(), scratch1.B16());
}

void ShaderEmitter::recADD(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	const u32 src1 = getBits<12, 7>(instruction);
	const u32 src2 = getBits<7, 5>(instruction);
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	loadRegister<1>(src1_vec, shader, src1, idx, operandDescriptor);
	loadRegister<2>(src2_vec, shader, src2, 0, operandDescriptor);
	FADD(src1_vec.S4(), src1_vec.S4(), src2_vec.S4());
	storeRegister(src1_vec, shader, dest, operandDescriptor);
}

void ShaderEmitter::recMAX(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	const u32 src1 = getBits<12, 7>(instruction);
	const u32 src2 = getBits<7, 5>(instruction);
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	loadRegister<1>(src1_vec, shader, src1, idx, operandDescriptor);
	loadRegister<2>(src2_vec, shader, src2, 0, operandDescriptor);
	FMAX(src1_vec.S4(), src1_vec.S4(), src2_vec.S4());
	storeRegister(src1_vec, shader, dest, operandDescriptor);
}

void ShaderEmitter::recMIN(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	const u32 src1 = getBits<12, 7>(instruction);
	const u32 src2 = getBits<7, 5>(instruction);
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	loadRegister<1>(src1_vec, shader, src1, idx, operandDescriptor);
	loadRegister<2>(src2_vec, shader, src2, 0, operandDescriptor);
	FMIN(src1_vec.S4(), src1_vec.S4(), src2_vec.S4());
	storeRegister(src1_vec, shader, dest, operandDescriptor);
}

void ShaderEmitter::recMUL(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	const u32 src1 = getBits<12, 7>(instruction);
	const u32 src2 = getBits<7, 5>(instruction);  // src2 coming first because PICA moment
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	loadRegister<1>(src1_vec, shader, src1, idx, operandDescriptor);
	loadRegister<2>(src2_vec, shader, src2, 0, operandDescriptor);

	if constexpr (useSafeMUL) {
		emitSafeMUL(src1_vec, src2_vec, scratch1);
	} else {
		FMUL(src1_vec.S4(), src1_vec.S4(), src2_vec.S4());
	}

	storeRegister(src1_vec, shader, dest, operandDescriptor);
}

void ShaderEmitter::recRCP(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	const u32 src = getBits<12, 7>(instruction);
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);
	const u32 writeMask = operandDescriptor & 0xf;

	loadRegister<1>(src1_vec, shader, src, idx, operandDescriptor);  // Load source 1 into scratch1
	FDIV(src1_vec.toS(), onesVector.toS(), src1_vec.toS());          // src1 = 1.0 / src1

	// If we only write back the x component to the result, we needn't perform a shuffle to do res = res.xxxx
	// Otherwise we do
	if (writeMask != 0x8) {                       // Copy bottom lane to all lanes if we're not simply writing back x
		DUP(src1_vec.S4(), src1_vec.Selem()[0]);  // src1_vec = src1_vec.xxxx
	}

	storeRegister(src1_vec, shader, dest, operandDescriptor);
}

void ShaderEmitter::recRSQ(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	const u32 src = getBits<12, 7>(instruction);
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);
	const u32 writeMask = operandDescriptor & 0xf;
	constexpr bool useAccurateRSQ = true;

	loadRegister<1>(src1_vec, shader, src, idx, operandDescriptor);  // Load source 1 into scratch1

	// Compute reciprocal square root approximation
	// TODO: Should this use frsqte or fsqrt+div? The former is faster but less accurate
	// PICA RSQ uses f24 precision though, so it'll be inherently innacurate, and it's likely using an inaccurate approximation too, seeing as
	// It doesn't have regular sqrt/div instructions.
	// For now, we default to accurate inverse square root
	if constexpr (useAccurateRSQ) {
		FSQRT(src1_vec.toS(), src1_vec.toS());                   // src1 = sqrt(src1), scalar
		FDIV(src1_vec.toS(), onesVector.toS(), src1_vec.toS());  // Now invert src1
	} else {
		FRSQRTE(src1_vec.toS(), src1_vec.toS());  // Much nicer
	}

	// If we only write back the x component to the result, we needn't perform a shuffle to do res = res.xxxx
	// Otherwise we do
	if (writeMask != 0x8) {                       // Copy bottom lane to all lanes if we're not simply writing back x
		DUP(src1_vec.S4(), src1_vec.Selem()[0]);  // src1_vec = src1_vec.xxxx
	}

	storeRegister(src1_vec, shader, dest, operandDescriptor);
}

void ShaderEmitter::recMAD(const PICAShader& shader, u32 instruction) {
	const bool isMADI = getBit<29>(instruction) == 0;

	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x1f];
	const u32 src1 = getBits<17, 5>(instruction);
	const u32 src2 = isMADI ? getBits<12, 5>(instruction) : getBits<10, 7>(instruction);
	const u32 src3 = isMADI ? getBits<5, 7>(instruction) : getBits<5, 5>(instruction);
	const u32 idx = getBits<22, 2>(instruction);
	const u32 dest = getBits<24, 5>(instruction);

	loadRegister<1>(src1_vec, shader, src1, 0, operandDescriptor);
	loadRegister<2>(src2_vec, shader, src2, isMADI ? 0 : idx, operandDescriptor);
	loadRegister<3>(src3_vec, shader, src3, isMADI ? idx : 0, operandDescriptor);

	if constexpr (useSafeMUL) {
		emitSafeMUL(src1_vec, src2_vec, scratch1);
		FADD(src3_vec.S4(), src3_vec.S4(), src1_vec.S4());
	} else {
		FMLA(src3_vec.S4(), src1_vec.S4(), src2_vec.S4());
	}
	storeRegister(src3_vec, shader, dest, operandDescriptor);
}

void ShaderEmitter::recSLT(const PICAShader& shader, u32 instruction) {
	const bool isSLTI = (instruction >> 26) == ShaderOpcodes::SLTI;
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];

	const u32 src1 = isSLTI ? getBits<14, 5>(instruction) : getBits<12, 7>(instruction);
	const u32 src2 = isSLTI ? getBits<7, 7>(instruction) : getBits<7, 5>(instruction);
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	loadRegister<1>(src1_vec, shader, src1, isSLTI ? 0 : idx, operandDescriptor);
	loadRegister<2>(src2_vec, shader, src2, isSLTI ? idx : 0, operandDescriptor);
	// Set each lane of SRC1 to FFFFFFFF if src2 > src1, else to 0. NEON does not have FCMLT so we use FCMGT with inverted operands
	// This is more or less a direct port of the relevant x64 JIT code
	FCMGT(src1_vec.S4(), src2_vec.S4(), src1_vec.S4());
	AND(src1_vec.B16(), src1_vec.B16(), onesVector.B16());  // AND with vec4(1.0) to convert the FFFFFFFF lanes into 1.0
	storeRegister(src1_vec, shader, dest, operandDescriptor);
}

void ShaderEmitter::recSGE(const PICAShader& shader, u32 instruction) {
	const bool isSGEI = (instruction >> 26) == ShaderOpcodes::SGEI;
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];

	const u32 src1 = isSGEI ? getBits<14, 5>(instruction) : getBits<12, 7>(instruction);
	const u32 src2 = isSGEI ? getBits<7, 7>(instruction) : getBits<7, 5>(instruction);
	const u32 idx = getBits<19, 2>(instruction);
	const u32 dest = getBits<21, 5>(instruction);

	loadRegister<1>(src1_vec, shader, src1, isSGEI ? 0 : idx, operandDescriptor);
	loadRegister<2>(src2_vec, shader, src2, isSGEI ? idx : 0, operandDescriptor);
	// Set each lane of SRC1 to FFFFFFFF if src1 >= src2, else to 0.
	// This is more or less a direct port of the relevant x64 JIT code
	FCMGE(src1_vec.S4(), src1_vec.S4(), src2_vec.S4());
	AND(src1_vec.B16(), src1_vec.B16(), onesVector.B16());  // AND with vec4(1.0) to convert the FFFFFFFF lanes into 1.0
	storeRegister(src1_vec, shader, dest, operandDescriptor);
}

void ShaderEmitter::recCMP(const PICAShader& shader, u32 instruction) {
	const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
	const u32 src1 = getBits<12, 7>(instruction);
	const u32 src2 = getBits<7, 5>(instruction);  // src2 coming first because PICA moment
	const u32 idx = getBits<19, 2>(instruction);
	const u32 cmpY = getBits<21, 3>(instruction);
	const u32 cmpX = getBits<24, 3>(instruction);

	loadRegister<1>(src1_vec, shader, src1, idx, operandDescriptor);
	loadRegister<2>(src2_vec, shader, src2, 0, operandDescriptor);

	// Map from PICA condition codes (used as index) to x86 condition codes
	// We treat invalid condition codes as "always" as suggested by 3DBrew
	static constexpr std::array<oaknut::Cond, 8> conditionCodes = {
		oaknut::util::EQ, oaknut::util::NE, oaknut::util::LT, oaknut::util::LE,
		oaknut::util::GT, oaknut::util::GE, oaknut::util::AL, oaknut::util::AL,
	};

	static_assert(sizeof(shader.cmpRegister[0]) == 1 && sizeof(shader.cmpRegister) == 2);  // The code below relies on bool being 1 byte exactly
	const size_t cmpRegXOffset = uintptr_t(&shader.cmpRegister[0]) - uintptr_t(&shader);

	// NEON doesn't have SIMD comparisons to do fun stuff with like on x64
	FCMP(src1_vec.toS(), src2_vec.toS());
	CSET(W0, conditionCodes[cmpX]);

	// Compare Y components, which annoyingly enough can't be done without moving
	MOV(scratch1.toS(), src1_vec.Selem()[1]);
	MOV(scratch2.toS(), src2_vec.Selem()[1]);
	FCMP(scratch1.toS(), scratch2.toS());
	CSET(W1, conditionCodes[cmpY]);

	// Merge the booleans and write them back in one STRh
	ORR(W0, W0, W1, LogShift::LSL, 8);
	STRH(W0, statePointer, cmpRegXOffset);
}

void ShaderEmitter::checkBoolUniform(const PICAShader& shader, u32 instruction) {
	const u32 bit = getBits<22, 4>(instruction);  // Bit of the bool uniform to check
	const uintptr_t boolUniformOffset = uintptr_t(&shader.boolUniform) - uintptr_t(&shader);

	LDRH(W0, statePointer, boolUniformOffset);  // Load bool uniform into w0
	TST(W0, 1 << bit);                          // Check if bit is set
}

void ShaderEmitter::checkCmpRegister(const PICAShader& shader, u32 instruction) {
	static_assert(sizeof(bool) == 1 && sizeof(shader.cmpRegister) == 2);  // The code below relies on bool being 1 byte exactly
	const size_t cmpRegXOffset = uintptr_t(&shader.cmpRegister[0]) - uintptr_t(&shader);
	const size_t cmpRegYOffset = cmpRegXOffset + sizeof(bool);

	const u32 condition = getBits<22, 2>(instruction);
	const uint refY = getBit<24>(instruction);
	const uint refX = getBit<25>(instruction);

	// refX in the bottom byte, refY in the top byte. This is done for condition codes 0 and 1 which check both x and y, so we can emit a single
	// instruction that checks both
	const u16 refX_refY_merged = refX | (refY << 8);

	switch (condition) {
		case 0:  // Either cmp register matches
			LDRB(W0, statePointer, cmpRegXOffset);
			LDRB(W1, statePointer, cmpRegYOffset);

			// Check if x matches refX
			CMP(W0, refX);
			CSET(W0, EQ);

			// Check if y matches refY
			CMP(W1, refY);
			CSET(W1, EQ);

			// Set Z to 1 if at least one of them matches
			ORR(W0, W0, W1);
			CMP(W0, 1);
			break;
		case 1:  // Both cmp registers match
			LDRH(W0, statePointer, cmpRegXOffset);

			// If ref fits in 8 bits, use a single CMP, otherwise move into register and then CMP
			if (refX_refY_merged <= 0xff) {
				CMP(W0, refX_refY_merged);
			} else {
				MOV(W1, refX_refY_merged);
				CMP(W0, W1);
			}
			break;
		case 2:  // At least cmp.x matches
			LDRB(W0, statePointer, cmpRegXOffset);
			CMP(W0, refX);
			break;
		default:  // At least cmp.y matches
			LDRB(W0, statePointer, cmpRegYOffset);
			CMP(W0, refY);
			break;
	}
}

void ShaderEmitter::recCALL(const PICAShader& shader, u32 instruction) {
	const u32 num = instruction & 0xff;
	const u32 dest = getBits<10, 12>(instruction);

	// Push return PC as stack parameter. This is a decently fast solution and Citra does the same but we should probably switch to a proper PICA-like
	// Callstack, because it's not great to have an infinitely expanding call stack
	MOV(X0, dest + num);
	// Push return PC + current link register so that we'll be able to return later
	STP(X0, X30, SP, PRE_INDEXED, -16);
	// Call subroutine, Oaknut will update the label if it hasn't been initialized yet
	BL(instructionLabels[dest]);

	// Fetch original LR and return. This also restores SP to its original value, discarding the return guard into XZR
	LDP(XZR, X30, SP, POST_INDEXED, 16);
}

void ShaderEmitter::recCALLC(const PICAShader& shader, u32 instruction) {
	Label skipCall;

	// z is 1 if the call should be taken, 0 otherwise
	checkCmpRegister(shader, instruction);
	B(NE, skipCall);
	recCALL(shader, instruction);

	l(skipCall);
}

void ShaderEmitter::recCALLU(const PICAShader& shader, u32 instruction) {
	Label skipCall;

	// z is 0 if the call should be taken, 1 otherwise
	checkBoolUniform(shader, instruction);
	B(EQ, skipCall);
	recCALL(shader, instruction);

	l(skipCall);
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
	B(NE, elseBlock);
	compileUntil(shader, dest);

	if (num == 0) {  // Else block is empty,
		l(elseBlock);
	} else {       // Else block is NOT empty
		B(endIf);  // Skip executing the else branch if the if branch was ran
		l(elseBlock);
		compileUntil(shader, dest + num);
		l(endIf);
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
	B(EQ, elseBlock);
	compileUntil(shader, dest);

	if (num == 0) {  // Else block is empty,
		l(elseBlock);
	} else {       // Else block is NOT empty
		B(endIf);  // Skip executing the else branch if the if branch was ran
		l(elseBlock);
		compileUntil(shader, dest + num);
		l(endIf);
	}
}

void ShaderEmitter::recJMPC(const PICAShader& shader, u32 instruction) {
	const u32 dest = getBits<10, 12>(instruction);

	Label& l = instructionLabels[dest];
	// Z is 1 if the comparison is true
	checkCmpRegister(shader, instruction);
	B(EQ, l);
}

void ShaderEmitter::recJMPU(const PICAShader& shader, u32 instruction) {
	bool jumpIfFalse = instruction & 1;  // If the LSB is 0 we want to compare to true, otherwise compare to false
	const u32 dest = getBits<10, 12>(instruction);

	Label& l = instructionLabels[dest];
	// Z is 0 if the uniform is true
	checkBoolUniform(shader, instruction);

	if (jumpIfFalse) {
		B(EQ, l);
	} else {
		B(NE, l);
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

	LDRB(W0, statePointer, uniformOffset);                   // W0 = loop iteration count
	LDRB(W1, statePointer, uniformOffset + sizeof(u8));      // W1 = initial loop counter value
	LDRB(W2, statePointer, uniformOffset + 2 * sizeof(u8));  // W2 = Loop increment

	ADD(W0, W0, 1);                        // The iteration count is actually uniform.x + 1
	STR(W1, statePointer, loopRegOffset);  // Set loop counter

	// Push loop iteration counter & loop increment
	// TODO: This might break if an instruction in a loop decides to yield...
	STP(X0, X2, SP, PRE_INDEXED, -16);

	Label loopStart, loopEnd;
	l(loopStart);
	compileUntil(shader, dest + 1);

	const size_t stackOffsetOfLoopIncrement = 0;
	const size_t stackOffsetOfIterationCounter = stackOffsetOfLoopIncrement + 8;

	LDP(X0, X2, SP);                       // W0 = loop iteration, W2 = loop increment
	LDR(W1, statePointer, loopRegOffset);  // W1 = loop register

	// Increment loop counter
	ADD(W1, W1, W2);
	STR(W1, statePointer, loopRegOffset);
	// Subtract 1 from loop iteration counter,
	SUBS(W0, W0, 1);
	B(EQ, loopEnd);

	// Loop hasn't ended: Write back new iteration counter and go back to the start
	STR(X0, SP);
	B(loopStart);

	l(loopEnd);
	// Remove the stuff we pushed on the stack earlier
	ADD(SP, SP, 16);
	loopLevel--;
}

void ShaderEmitter::recEND(const PICAShader& shader, u32 instruction) {
	// Fetch original LR and return. This also restores SP to its original value, discarding the return guard into XZR
	LDP(XZR, X30, SP, POST_INDEXED, 16);
	RET();
}

#endif
