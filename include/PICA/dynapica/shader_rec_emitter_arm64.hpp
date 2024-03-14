#pragma once

// Only do anything if we're on an x64 target with JIT support enabled
#if defined(PANDA3DS_DYNAPICA_SUPPORTED) && defined(PANDA3DS_ARM64_HOST)
#include <array>
#include <oaknut/code_block.hpp>
#include <oaknut/oaknut.hpp>

#include "PICA/shader.hpp"
#include "helpers.hpp"
#include "logger.hpp"

class ShaderEmitter : private oaknut::CodeBlock, public oaknut::CodeGenerator {
	static constexpr size_t executableMemorySize = PICAShader::maxInstructionCount * 96;  // How much executable memory to alloc for each shader
	// Allocate some extra space as padding for security purposes in the extremely unlikely occasion we manage to overflow the above size
	static constexpr size_t allocSize = executableMemorySize + 0x1000;

	// If the swizzle field is this value then the swizzle pattern is .xyzw so we don't need a shuffle
	static constexpr uint noSwizzle = 0x1B;

	using f24 = Floats::f24;
	using vec4f = std::array<f24, 4>;

	// An array of labels (incl pointers) to each compiled (to x64) PICA instruction
	std::array<oaknut::Label, PICAShader::maxInstructionCount> instructionLabels;
	// A vector of PCs that can potentially return based on the state of the PICA callstack.
	// Filled before compiling a shader by scanning the code for call instructions
	std::vector<u32> returnPCs;

	// An array of 128-bit masks for blending registers together to perform masked writes.
	// Eg for writing only the x and y components, the mask is 0x00000000'00000000'FFFFFFFF'FFFF
	oaknut::Label blendMasks;

	u32 recompilerPC = 0;  // PC the recompiler is currently recompiling @
	u32 loopLevel = 0;     // The current loop nesting level (0 = not in a loop)

	// Shows whether the loaded shader has any log2 and exp2 instructions
	bool codeHasLog2 = false;
	bool codeHasExp2 = false;

	oaknut::Label log2Func, exp2Func;
	oaknut::Label emitLog2Func();
	oaknut::Label emitExp2Func();

	// Emit a PICA200-compliant multiplication that handles "0 * inf = 0"
	void emitSafeMUL(oaknut::QReg src1, oaknut::QReg src2, oaknut::QReg scratch0);

	template <typename T>
	T getLabelPointer(const oaknut::Label& label) {
		auto pointer = reinterpret_cast<u8*>(oaknut::CodeBlock::ptr()) + label.offset();
		return reinterpret_cast<T>(pointer);
	}

	// Compile all instructions from [current recompiler PC, end)
	void compileUntil(const PICAShader& shaderUnit, u32 endPC);
	// Compile instruction "instr"
	void compileInstruction(const PICAShader& shaderUnit);

	bool isCall(u32 instruction) {
		const u32 opcode = instruction >> 26;
		return (opcode == ShaderOpcodes::CALL) || (opcode == ShaderOpcodes::CALLC) || (opcode == ShaderOpcodes::CALLU);
	}

	// Scan the shader code for call instructions to fill up the returnPCs vector before starting compilation
	// We also scan for log2/exp2 instructions to see whether to emit the relevant functions
	void scanCode(const PICAShader& shaderUnit);

	// Load register with number "srcReg" indexed by index "idx" into the arm64 register "reg"
	template <int sourceIndex>
	void loadRegister(oaknut::QReg dest, const PICAShader& shader, u32 src, u32 idx, u32 operandDescriptor);
	void storeRegister(oaknut::QReg source, const PICAShader& shader, u32 dest, u32 operandDescriptor);

	const vec4f& getSourceRef(const PICAShader& shader, u32 src);
	const vec4f& getDestRef(const PICAShader& shader, u32 dest);

	// Check the value of the cmp register for instructions like ifc and callc
	// Result is returned in the zero flag. If the comparison is true then zero == 1, else zero == 0
	void checkCmpRegister(const PICAShader& shader, u32 instruction);

	// Check the value of the bool uniform for instructions like ifu and callu
	// Result is returned in the zero flag. If the comparison is true then zero == 0, else zero == 1 (Opposite of checkCmpRegister)
	void checkBoolUniform(const PICAShader& shader, u32 instruction);

	// Instruction recompilation functions
	void recADD(const PICAShader& shader, u32 instruction);
	void recCALL(const PICAShader& shader, u32 instruction);
	void recCALLC(const PICAShader& shader, u32 instruction);
	void recCALLU(const PICAShader& shader, u32 instruction);
	void recCMP(const PICAShader& shader, u32 instruction);
	void recDP3(const PICAShader& shader, u32 instruction);
	void recDP4(const PICAShader& shader, u32 instruction);
	void recDPH(const PICAShader& shader, u32 instruction);
	void recEMIT(const PICAShader& shader, u32 instruction);
	void recEND(const PICAShader& shader, u32 instruction);
	void recEX2(const PICAShader& shader, u32 instruction);
	void recFLR(const PICAShader& shader, u32 instruction);
	void recIFC(const PICAShader& shader, u32 instruction);
	void recIFU(const PICAShader& shader, u32 instruction);
	void recJMPC(const PICAShader& shader, u32 instruction);
	void recJMPU(const PICAShader& shader, u32 instruction);
	void recLG2(const PICAShader& shader, u32 instruction);
	void recLOOP(const PICAShader& shader, u32 instruction);
	void recMAD(const PICAShader& shader, u32 instruction);
	void recMAX(const PICAShader& shader, u32 instruction);
	void recMIN(const PICAShader& shader, u32 instruction);
	void recMOVA(const PICAShader& shader, u32 instruction);
	void recMOV(const PICAShader& shader, u32 instruction);
	void recMUL(const PICAShader& shader, u32 instruction);
	void recRCP(const PICAShader& shader, u32 instruction);
	void recRSQ(const PICAShader& shader, u32 instruction);
	void recSETEMIT(const PICAShader& shader, u32 instruction);
	void recSGE(const PICAShader& shader, u32 instruction);
	void recSLT(const PICAShader& shader, u32 instruction);

	MAKE_LOG_FUNCTION(log, shaderJITLogger)

  public:
	// Callback type used for instructions
	using InstructionCallback = const void (*)(PICAShader& shaderUnit);
	// Callback type used for the JIT prologue. This is what the caller will call
	using PrologueCallback = const void (*)(PICAShader& shaderUnit, InstructionCallback cb);

	PrologueCallback prologueCb = nullptr;

	// Initialize our emitter with "allocSize" bytes of memory allocated for the code buffer
	ShaderEmitter() : oaknut::CodeBlock(allocSize), oaknut::CodeGenerator(oaknut::CodeBlock::ptr()) {}

	// PC must be a valid entrypoint here. It doesn't have that much overhead in this case, so we use std::array<>::at() to assert it does
	InstructionCallback getInstructionCallback(u32 pc) { return getLabelPointer<InstructionCallback>(instructionLabels.at(pc)); }

	PrologueCallback getPrologueCallback() { return prologueCb; }
	void compile(const PICAShader& shaderUnit);
};

#endif  // arm64 recompiler check
