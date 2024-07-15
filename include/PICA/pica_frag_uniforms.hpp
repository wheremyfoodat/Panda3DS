#pragma once
#include <array>
#include <type_traits>

#include "helpers.hpp"

namespace PICA {
	struct FragmentUniforms {
		using vec3 = std::array<float, 3>;
		using vec4 = std::array<float, 4>;
		static constexpr usize tevStageCount = 6;

		s32 alphaReference;

		alignas(16) vec4 constantColors[tevStageCount];
		alignas(16) vec4 tevBufferColor;
	};
}  // namespace PICA