#pragma once
#include <string>

#include "PICA/shader.hpp"
#include "PICA/shader_gen_types.hpp"

namespace PICA::ShaderGen {
	std::string decompileShader(PICAShader& shaderUnit);
}