#pragma once
#include <array>
#include <cstddef>
#include <type_traits>

#include "helpers.hpp"

namespace PICA {
	struct LightUniform {
		using vec3 = std::array<float, 3>;

		// std140 requires vec3s be aligned to 16 bytes
		alignas(16) vec3 specular0;
		alignas(16) vec3 specular1;
		alignas(16) vec3 diffuse;
		alignas(16) vec3 ambient;
		alignas(16) vec3 position;
		alignas(16) vec3 spotlightDirection;
		
		float distanceAttenuationBias;
		float distanceAttenuationScale;
	};

	struct FragmentUniforms {
		using vec3 = std::array<float, 3>;
		using vec4 = std::array<float, 4>;
		static constexpr usize tevStageCount = 6;

		s32 alphaReference;
		float depthScale;
		float depthOffset;

		alignas(16) vec4 constantColors[tevStageCount];
		alignas(16) vec4 tevBufferColor;
		alignas(16) vec4 clipCoords;

		// Note: We upload these as a u32 and decode on GPU.
		// Particularly the fog colour since fog is really uncommon and it doesn't matter if we decode on GPU.
		u32 globalAmbientLight;
		u32 fogColor;
		// NOTE: THIS MUST BE LAST so that if lighting is disabled we can potentially omit uploading it
		LightUniform lightUniforms[8];
	};

	// Assert that lightUniforms is the last member of the structure
	static_assert(offsetof(FragmentUniforms, lightUniforms) + 8 * sizeof(LightUniform) == sizeof(FragmentUniforms));
}  // namespace PICA