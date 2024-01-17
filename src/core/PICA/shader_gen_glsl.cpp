#include "PICA/shader_gen.hpp"
using namespace PICA::ShaderGen;

std::string FragmentGenerator::generate(const PICARegs& regs) {
	std::string ret = "";

	switch (api) {
		case API::GL: ret += "#version 410 core"; break;
		case API::GLES: ret += "#version 300 es"; break;
		default: break;
	}

	// Input and output attributes
	ret += R"(
		in vec3 v_tangent;
		in vec3 v_normal;
		in vec3 v_bitangent;
		in vec4 v_colour;
		in vec3 v_texcoord0;
		in vec2 v_texcoord1;
		in vec3 v_view;
		in vec2 v_texcoord2;
		flat in vec4 v_textureEnvColor[6];
		flat in vec4 v_textureEnvBufferColor;

		out vec4 fragColour;
	)";

	return ret;
}