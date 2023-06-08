#include "PICA/dynapica/shader_rec.hpp"
#include "cityhash.hpp"

#ifdef PANDA3DS_SHADER_JIT_SUPPORTED
void ShaderJIT::reset() {
	cache.clear();
}

void ShaderJIT::prepare(PICAShader& shaderUnit) {
	shaderUnit.pc = shaderUnit.entrypoint;
	// We construct a shader hash from both the code and operand descriptor hashes
	// This is so that if only one of them changes, we still properly recompile the shader
	// This code is inspired from how Citra solves this problem
	Hash hash = shaderUnit.getCodeHash() ^ shaderUnit.getOpdescHash();
	auto it = cache.find(hash);

	if (it == cache.end()) { // Block has not been compiled yet
		auto emitter = std::make_unique<ShaderEmitter>();
		emitter->compile(shaderUnit);
		cache.emplace_hint(it, hash, std::move(emitter));
	} else { // Block has been compiled and found, use it
		auto emitter = it->second.get();
	}
}
#endif // PANDA3DS_SHADER_JIT_SUPPORTED