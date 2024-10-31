#pragma once
#include <array>
#include <cassert>
#include <cstring>
#include <type_traits>
#include <unordered_map>

#include "PICA/pica_hash.hpp"
#include "PICA/regs.hpp"
#include "PICA/shader.hpp"
#include "bitfield.hpp"
#include "helpers.hpp"

namespace PICA {
	// Configuration struct used
	struct VertConfig {
		PICAHash::HashType shaderHash;
		PICAHash::HashType opdescHash;
		u32 entrypoint;

		// PICA registers for configuring shader output->fragment semantic mapping
		std::array<u32, 7> outmaps{};
		u16 outputMask;
		u8 outputCount;
		bool usingUbershader;

		// Pad to 56 bytes so that the compiler won't insert unnecessary padding, which in turn will affect our unordered_map lookup
		// As the padding will get hashed and memcmp'd...
		u32 pad{};

		bool operator==(const VertConfig& config) const {
			// Hash function and equality operator required by std::unordered_map
			return std::memcmp(this, &config, sizeof(VertConfig)) == 0;
		}

		VertConfig(PICAShader& shader, const std::array<u32, 0x300>& regs, bool usingUbershader) : usingUbershader(usingUbershader) {
			shaderHash = shader.getCodeHash();
			opdescHash = shader.getOpdescHash();
			entrypoint = shader.entrypoint;

			outputCount = regs[PICA::InternalRegs::ShaderOutputCount] & 7;
			outputMask = regs[PICA::InternalRegs::VertexShaderOutputMask];
			for (int i = 0; i < outputCount; i++) {
				// Mask out unused bits
				outmaps[i] = regs[PICA::InternalRegs::ShaderOutmap0 + i] & 0x1F1F1F1F;
			}
		}
	};
}  // namespace PICA

static_assert(sizeof(PICA::VertConfig) == 56);

// Override std::hash for our vertex config class
template <>
struct std::hash<PICA::VertConfig> {
	std::size_t operator()(const PICA::VertConfig& config) const noexcept { return PICAHash::computeHash((const char*)&config, sizeof(config)); }
};