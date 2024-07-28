#pragma once
#include <array>
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
				outmaps[i] = regs[PICA::InternalRegs::ShaderOutmap0 + i];
			}
		}
	};
}  // namespace PICA

// Override std::hash for our vertex config class
template <>
struct std::hash<PICA::VertConfig> {
	std::size_t operator()(const PICA::VertConfig& config) const noexcept { return PICAHash::computeHash((const char*)&config, sizeof(config)); }
};