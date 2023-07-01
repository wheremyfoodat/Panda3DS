#pragma once
#include <cstdint>
#include <cstddef>

// Defines to pick which hash algorithm to use for the PICA (For hashing shaders, etc)
// Please only define one of them
// Available algorithms:
// xxh3: 64-bit non-cryptographic hash using SIMD, default.
// Google CityHash64: 64-bit non-cryptographic hash, generated using regular 64-bit arithmetic

//#define PANDA3DS_PICA_CITYHASH
#define PANDA3DS_PICA_XXHASH3

namespace PICAHash {
	#if defined(PANDA3DS_PICA_CITYHASH) || defined(PANDA3DS_PICA_XXHASH3)
	using HashType = std::uint64_t;
	#endif

	HashType computeHash(const char* buf, std::size_t len);
} // namespace PICAHash 