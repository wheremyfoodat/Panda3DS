#pragma once
#include <string>

#include "PICA/gpu.hpp"
#include "PICA/regs.hpp"
#include "helpers.hpp"

namespace PICA::ShaderGen {
	// Graphics API this shader is targetting
	enum class API { GL, GLES, Vulkan };

	// Shading language to use (Only GLSL for the time being)
	enum class Language { GLSL };

	class FragmentGenerator {
		using PICARegs = std::array<u32, 0x300>;
		API api;
		Language language;

	  public:
		FragmentGenerator(API api, Language language) : api(api), language(language) {}
		std::string generate(const PICARegs& regs);
	};
};  // namespace PICA::ShaderGen