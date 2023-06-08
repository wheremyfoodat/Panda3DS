#pragma once

// Only do anything if we're on an x64 target with JIT support enabled
#if defined(PANDA3DS_DYNAPICA_SUPPORTED) && defined(PANDA3DS_X64_HOST)
#include "helpers.hpp"
#include "PICA/shader.hpp"
#include "xbyak/xbyak.h"
#include "x64_regs.hpp"

#include <vector>

class ShaderEmitter : public Xbyak::CodeGenerator {
	static constexpr size_t executableMemorySize = PICAShader::maxInstructionCount * 96; // How much executable memory to alloc for each shader
	// Allocate some extra space as padding for security purposes in the extremely unlikely occasion we manage to overflow the above size
	static constexpr size_t allocSize = executableMemorySize + 0x1000;

	// An array of labels (incl pointers) to each compiled (to x64) PICA instruction
	std::array<Xbyak::Label, PICAShader::maxInstructionCount> instructionLabels;
	// A vector of PCs that can potentially return based on the state of the PICA callstack.
	// Filled before compiling a shader by scanning the code for call instructions
	std::vector<u32> returnPCs;

	u32 recompilerPC; // PC the recompiler is currently recompiling @

	// Compile all instructions from [current recompiler PC, end)
	void compileUntil(const PICAShader& shaderUnit, u32 endPC);
	// Compile instruction "instr"
	void compileInstruction(const PICAShader& shaderUnit);

	bool isCall(u32 instruction) {
		const u32 opcode = instruction >> 26;
		return (opcode == ShaderOpcodes::CALL) || (opcode == ShaderOpcodes::CALLC) || (opcode == ShaderOpcodes::CALLU);
	}
	// Scan the shader code for call instructions to fill up the returnPCs vector before starting compilation
	void scanForCalls(const PICAShader& shader);

public:
	using InstructionCallback = const void(*)(PICAShader& shaderUnit); // Callback type used for instructions
	// Callback type used for the JIT prologue. This is what the caller will call
	using PrologueCallback = const void(*)(PICAShader& shaderUnit, InstructionCallback cb);
	PrologueCallback prologueCb;

	// Initialize our emitter with "allocSize" bytes of RWX memory
	ShaderEmitter() : Xbyak::CodeGenerator(allocSize) {}
	void compile(const PICAShader& shaderUnit);

	// PC must be a valid entrypoint here. It doesn't have that much overhead in this case, so we use std::array<>::at() to assert it does
	InstructionCallback getInstructionCallback(u32 pc) {
		return reinterpret_cast<InstructionCallback>(instructionLabels.at(pc).getAddress());
	}

	PrologueCallback getPrologueCallback() {
		return prologueCb;
	}
};

#endif // x64 recompiler check