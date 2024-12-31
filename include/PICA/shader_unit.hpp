#pragma once
#include "PICA/shader.hpp"

class ShaderUnit {
  public:
	PICAShader vs;  // Vertex shader
	PICAShader gs;  // Geometry shader

	ShaderUnit() : vs(ShaderType::Vertex), gs(ShaderType::Geometry) {}
	void reset();
};