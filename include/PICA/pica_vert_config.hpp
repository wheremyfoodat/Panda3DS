#pragma once
#include <array>
#include <cstring>
#include <type_traits>
#include <unordered_map>

#include "PICA/pica_hash.hpp"
#include "PICA/regs.hpp"
#include "bitfield.hpp"
#include "helpers.hpp"

namespace PICA {
	// Configuration struct used 
	struct VertConfig {
		PICAHash::HashType shaderHash;
		PICAHash::HashType opdescHash;
		u32 entrypoint;
		bool usingUbershader;

		bool operator==(const VertConfig& config) const {
			// Hash function and equality operator required by std::unordered_map
			return std::memcmp(this, &config, sizeof(VertConfig)) == 0;
		}
	};
}  // namespace PICA

// Override std::hash for our vertex config class
template <>
struct std::hash<PICA::VertConfig> {
	std::size_t operator()(const PICA::VertConfig& config) const noexcept { return PICAHash::computeHash((const char*)&config, sizeof(config)); }
};