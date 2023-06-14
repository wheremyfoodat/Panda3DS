#include "PICA/dynapica/shader_rec.hpp"
#include <bit>

#ifdef PANDA3DS_SHADER_JIT_SUPPORTED
void ShaderJIT::reset() {
	cache.clear();
}

void ShaderJIT::prepare(PICAShader& shaderUnit) {
	shaderUnit.pc = shaderUnit.entrypoint;
	// We combine the code and operand descriptor hashes into a single hash
	// This is so that if only one of them changes, we still properly recompile the shader
	// The combine does rotl(x, 1) ^ y for the merging instead of x ^ y because xor is commutative, hence creating possible collisions
	// re: https://github.com/wheremyfoodat/Panda3DS/pull/15#discussion_r1229925372
	Hash hash = std::rotl(shaderUnit.getCodeHash(), 1) ^ shaderUnit.getOpdescHash();
	auto it = cache.find(hash);

	if (it == cache.end()) { // Block has not been compiled yet
		auto emitter = std::make_unique<ShaderEmitter>();
		emitter->compile(shaderUnit);
		// Get pointer to callbacks
		entrypointCallback = emitter->getInstructionCallback(shaderUnit.entrypoint);
		prologueCallback = emitter->getPrologueCallback();

		cache.emplace_hint(it, hash, std::move(emitter));
	} else { // Block has been compiled and found, use it
		auto emitter = it->second.get();
		entrypointCallback = emitter->getInstructionCallback(shaderUnit.entrypoint);
		prologueCallback = emitter->getPrologueCallback();
	}
}
#endif // PANDA3DS_SHADER_JIT_SUPPORTED