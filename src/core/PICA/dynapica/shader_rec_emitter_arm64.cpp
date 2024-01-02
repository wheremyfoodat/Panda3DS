#if defined(PANDA3DS_DYNAPICA_SUPPORTED) && defined(PANDA3DS_ARM64_HOST)
#include "PICA/dynapica/shader_rec_emitter_arm64.hpp"

#include <bit>

using namespace Helpers;
using namespace oaknut;
using namespace oaknut::util;

// Similar to the x64 recompiler, we use an odd internal ABI, which abuses the fact that we'll very rarely be calling C++ functions
// So to avoid pushing and popping, we'll be making use of volatile registers as much as possible
static constexpr QReg scratch1 = Q0;
static constexpr QReg scratch2 = Q1;
static constexpr QReg src1_vec = Q2;
static constexpr QReg src2_vec = Q3;
static constexpr QReg src3_vec = Q4;

static constexpr XReg statePointer = X9;

void ShaderEmitter::compile(const PICAShader& shaderUnit) {
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
		Helpers::panic("Unimplemented return address for call instruction");
	}

	// Fetch instruction and inc PC
	const u32 instruction = shaderUnit.loadedShader[recompilerPC++];
	const u32 opcode = instruction >> 26;

	switch (opcode) {
		// case ShaderOpcodes::ADD: recADD(shaderUnit, instruction); break;
		// case ShaderOpcodes::CALL: recCALL(shaderUnit, instruction); break;
		// case ShaderOpcodes::CALLC: recCALLC(shaderUnit, instruction); break;
		// case ShaderOpcodes::CALLU: recCALLU(shaderUnit, instruction); break;
		// case ShaderOpcodes::CMP1:
		// case ShaderOpcodes::CMP2: recCMP(shaderUnit, instruction); break;
		// case ShaderOpcodes::DP3: recDP3(shaderUnit, instruction); break;
		// case ShaderOpcodes::DP4: recDP4(shaderUnit, instruction); break;
		// case ShaderOpcodes::DPH:
		// case ShaderOpcodes::DPHI: recDPH(shaderUnit, instruction); break;
		// case ShaderOpcodes::END: recEND(shaderUnit, instruction); break;
		// case ShaderOpcodes::EX2: recEX2(shaderUnit, instruction); break;
		// case ShaderOpcodes::FLR: recFLR(shaderUnit, instruction); break;
		// case ShaderOpcodes::IFC: recIFC(shaderUnit, instruction); break;
		// case ShaderOpcodes::IFU: recIFU(shaderUnit, instruction); break;
		// case ShaderOpcodes::JMPC: recJMPC(shaderUnit, instruction); break;
		// case ShaderOpcodes::JMPU: recJMPU(shaderUnit, instruction); break;
		// case ShaderOpcodes::LG2: recLG2(shaderUnit, instruction); break;
		// case ShaderOpcodes::LOOP: recLOOP(shaderUnit, instruction); break;
		case ShaderOpcodes::MOV: recMOV(shaderUnit, instruction); break;
		// case ShaderOpcodes::MOVA: recMOVA(shaderUnit, instruction); break;
		// case ShaderOpcodes::MAX: recMAX(shaderUnit, instruction); break;
		// case ShaderOpcodes::MIN: recMIN(shaderUnit, instruction); break;
		// case ShaderOpcodes::MUL: recMUL(shaderUnit, instruction); break;
		case ShaderOpcodes::NOP:
			break;
			// case ShaderOpcodes::RCP: recRCP(shaderUnit, instruction); break;
			// case ShaderOpcodes::RSQ: recRSQ(shaderUnit, instruction); break;

			// Unimplemented opcodes that don't seem to actually be used but exist in the binary
			// EMIT/SETEMIT are used in geometry shaders, however are sometimes found in vertex shaders?
			// case ShaderOpcodes::EMIT:
			// case ShaderOpcodes::SETEMIT:
			//	log("[ShaderJIT] Unknown PICA opcode: %02X\n", opcode);
			//	emitPrintLog(shaderUnit);
			//	break;

			// case ShaderOpcodes::BREAK:
			// case ShaderOpcodes::BREAKC: Helpers::warn("[Shader JIT] Unimplemented BREAK(C) instruction!"); break;

			// We consider both MAD and MADI to be the same instruction and decode which one we actually have in recMAD
			// case 0x30:
			// case 0x31:
			// case 0x32:
			// case 0x33:
			// case 0x34:
			// case 0x35:
			// case 0x36:
			// case 0x37:
			// case 0x38:
			// case 0x39:
			// case 0x3A:
			// case 0x3B:
			// case 0x3C:
			// case 0x3D:
			// case 0x3E:
			// case 0x3F: recMAD(shaderUnit, instruction); break;

			// case ShaderOpcodes::SLT:
			// case ShaderOpcodes::SLTI: recSLT(shaderUnit, instruction); break;

			// case ShaderOpcodes::SGE:
			// case ShaderOpcodes::SGEI: recSGE(shaderUnit, instruction); break;

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
					case 0xFF: DUP(dest.S4(), dest.Selem()[3]); break;  // .wwww
					default: Helpers::panic("Unimplemented swizzle pattern for loading");
				}

				// Negate the register if necessary
				if (negate) {
					FNEG(dest.S4(), dest.S4());
				}
				return;  // Return. Rest of the function handles indexing which is not used if index == 0
			}

		default: Helpers::panic("[ShaderJIT]: Unimplemented source index type %d", index);
	}

	Helpers::panic("Unimplemented indexed register load");
}

void ShaderEmitter::storeRegister(QReg source, const PICAShader& shader, u32 dest, u32 operandDescriptor) {
	const vec4f& destRef = getDestRef(shader, dest);
	const uintptr_t offset = uintptr_t(&destRef) - uintptr_t(&shader);  // Calculate offset of register from start of the state struct

	// Mask of which lanes to write
	u32 writeMask = operandDescriptor & 0xf;
	if (writeMask == 0xf) {  // No lanes are masked, just use STR
		STR(source, statePointer, offset);
	} else {
        LDR(scratch1, statePointer, offset); // Load current source
        Helpers::panic("Unimplemented: Storing to register with blending");
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

#endif