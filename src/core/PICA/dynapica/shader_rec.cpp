#include "PICA/dynapica/shader_rec.hpp"
#include "cityhash.hpp"

#ifdef PANDA3DS_SHADER_JIT_SUPPORTED
void ShaderJIT::reset() {
	cache.clear();
}

void ShaderJIT::prepare(PICAShader& shaderUnit) {
	// We construct a shader hash from both the code and operand descriptor hashes
	// This is so that if only one of them changes, we still properly recompile the shader
	Hash hash = shaderUnit.getCodeHash() ^ shaderUnit.getOpdescHash();
}
#endif // PANDA3DS_SHADER_JIT_SUPPORTED