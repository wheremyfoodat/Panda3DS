#pragma once
#include "PICA/shader.hpp"

class ShaderUnit {

public:
	PICAShader<ShaderType::Vertex> vs; // Vertex shader
	PICAShader<ShaderType::Geometry> gs; // Geometry shader

	void reset();
};