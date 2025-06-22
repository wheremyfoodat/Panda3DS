#pragma once

namespace PICA::ShaderGen {
	// Graphics API this shader is targetting
	enum class API { GL, GLES, Vulkan, Metal };

	// Shading language to use
	enum class Language { GLSL, MSL };
}  // namespace PICA::ShaderGen
