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
	struct OutputConfig {
		union {
			u32 raw;
			// Merge the enable + compare function into 1 field to avoid duplicate shaders
			// enable == off means a CompareFunction of Always
			BitField<0, 3, CompareFunction> alphaTestFunction;
		};
	};

	struct TextureConfig {
		u32 texUnitConfig;
		u32 texEnvUpdateBuffer;

		// TODO: This should probably be a uniform
		u32 texEnvBufferColor;

		// There's 6 TEV stages, and each one is configured via 5 word-sized registers
		std::array<u32, 5 * 6> tevConfigs;
	};

	struct FragmentConfig {
		OutputConfig outConfig;
		TextureConfig texConfig;

		bool operator==(const FragmentConfig& config) const {
			// Hash function and equality operator required by std::unordered_map
			return std::memcmp(this, &config, sizeof(FragmentConfig)) == 0;
		}
	};

	static_assert(
		std::has_unique_object_representations<OutputConfig>() && std::has_unique_object_representations<TextureConfig>() &&
		std::has_unique_object_representations<FragmentConfig>()
	);
}  // namespace PICA

// Override std::hash for our fragment config class
template <>
struct std::hash<PICA::FragmentConfig> {
	std::size_t operator()(const PICA::FragmentConfig& config) const noexcept { return PICAHash::computeHash((const char*)&config, sizeof(config)); }
};