#include "PICA/pica_hash.hpp"
#include "PICA/shader.hpp"

#ifdef PANDA3DS_PICA_CITYHASH
#include "cityhash.hpp"
#elif defined(PANDA3DS_PICA_XXHASH3)
#include "xxhash/xxhash.h"
#else
#error No hashing algorithm defined for the PICA (See pica_hash.hpp for details)
#endif

PICAHash::HashType PICAHash::computeHash(const char* buf, std::size_t len) {
#ifdef PANDA3DS_PICA_CITYHASH
	return CityHash::CityHash64(buf, len);
#elif defined(PANDA3DS_PICA_XXHASH3)
	return XXH3_64bits(buf, len);
#else
#error No hashing algorithm defined for PICAHash::computeHash
#endif
}

PICAShader::Hash PICAShader::getCodeHash() {
	// Hash the code again if the code changed
	if (codeHashDirty) {
		codeHashDirty = false;
		lastCodeHash = PICAHash::computeHash((const char*)&loadedShader[0], loadedShader.size() * sizeof(loadedShader[0]));
	}

	// Return the code hash
	return lastCodeHash;
}

PICAShader::Hash PICAShader::getOpdescHash() {
	// Hash the code again if the operand descriptors changed
	if (opdescHashDirty) {
		opdescHashDirty = false;
		lastOpdescHash = PICAHash::computeHash((const char*)&operandDescriptors[0], operandDescriptors.size() * sizeof(operandDescriptors[0]));
	}

	// Return the code hash
	return lastOpdescHash;
}