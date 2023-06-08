#pragma once

// Only do anything if we're on an x64 target with JIT support enabled
#if defined(PANDA3DS_DYNAPICA_SUPPORTED) && defined(PANDA3DS_X64_HOST)
#include "helpers.hpp"
#include "PICA/shader.hpp"
#include "xbyak/xbyak.h"
#include "x64_regs.hpp"

class ShaderEmitter : public Xbyak::CodeGenerator {
	static constexpr size_t executableMemorySize = PICAShader::maxInstructionCount * 96; // How much executable memory to alloc for each shader
	// Allocate some extra space as padding for security purposes in the extremely unlikely occasion we manage to overflow the above size
	static constexpr size_t allocSize = executableMemorySize + 0x1000;

public:
	using Callback = void(*)(const PICAShader& shaderUnit);

	// Initialize our emitter with "allocSize" bytes of RWX memory
	ShaderEmitter() : Xbyak::CodeGenerator(allocSize) {}
	void compile(const PICAShader& shaderUnit);
};

#endif // x64 recompiler check