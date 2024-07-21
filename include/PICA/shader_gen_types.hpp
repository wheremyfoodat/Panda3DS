#pragma once

namespace PICA::ShaderGen {
	// Graphics API this shader is targetting
	enum class API { GL, GLES, Vulkan };

	// Shading language to use (Only GLSL for the time being)
	enum class Language { GLSL };
}  // namespace PICA::ShaderGen