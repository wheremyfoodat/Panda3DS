#pragma once
#include "PICA/shader.hpp"

#if defined(PANDA3DS_DYNAPICA_SUPPORTED) && defined(PANDA3DS_X64_HOST)
#define PANDA3DS_SHADER_JIT_SUPPORTED
#include <memory>
#include <unordered_map>

#ifdef PANDA3DS_X64_HOST
#include "shader_rec_emitter_x64.hpp"
#endif
#endif

class ShaderJIT {
#ifdef PANDA3DS_SHADER_JIT_SUPPORTED
	using Hash = PICAShader::Hash;
	using ShaderCache = std::unordered_map<Hash, std::unique_ptr<ShaderEmitter>>;
	ShaderEmitter::PrologueCallback prologueCallback;
	ShaderEmitter::InstructionCallback entrypointCallback;

	ShaderCache cache;
#endif

  public:
#ifdef PANDA3DS_SHADER_JIT_SUPPORTED
	// Call this before starting to process a batch of vertices
	// This will read the PICA config (uploaded shader and shader operand descriptors) and search if we've already compiled this shader
	// If yes, it sets it as the active shader. if not, then it compiles it, adds it to the cache, and sets it as active,
	// The caller must make sure the entrypoint has been properly set beforehand
	void prepare(PICAShader& shaderUnit);
	void reset();
	void run(PICAShader& shaderUnit) { prologueCallback(shaderUnit, entrypointCallback); }

	static constexpr bool isAvailable() { return true; }
#else
	void prepare(PICAShader& shaderUnit) {
		Helpers::panic("Vertex Loader JIT: Tried to run ShaderJIT::Prepare on platform that does not support shader jit");
	}

	void run(PICAShader& shaderUnit) {
		Helpers::panic("Vertex Loader JIT: Tried to run ShaderJIT::Run on platform that does not support shader jit");
	}

	// Define dummy callback. This should never be called if the shader JIT is not supported
	using Callback = void (*)(PICAShader& shaderUnit);
	Callback activeShaderCallback = nullptr;

	void reset() {}
	static constexpr bool isAvailable() { return false; }
#endif
};