#include "PICA/shader_gen.hpp"
using namespace PICA;
using namespace PICA::ShaderGen;

std::string FragmentGenerator::generate(const PICARegs& regs) {
	std::string ret = "";

	switch (api) {
		case API::GL: ret += "#version 410 core"; break;
		case API::GLES: ret += "#version 300 es"; break;
		default: break;
	}

	bool unimplementedFlag = false;

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
		uniform sampler2D u_tex0;
		uniform sampler2D u_tex1;
		uniform sampler2D u_tex2;
		uniform sampler1DArray u_tex_lighting_lut;

		vec4 tevSources[16];
		vec4 tevNextPreviousBuffer;

		vec3 regToColor(uint reg) {
			// Normalization scale to convert from [0...255] to [0.0...1.0]
			const float scale = 1.0 / 255.0;

			return scale * vec3(float(bitfieldExtract(reg, 20, 8)), float(bitfieldExtract(reg, 10, 8)), float(bitfieldExtract(reg, 00, 8)));
		}
	)";

	// Emit main function for fragment shader
	// When not initialized, source 13 is set to vec4(0.0) and 15 is set to the vertex colour
	ret += R"(
		void main() {
			tevSources[0] = v_colour;
			tevSources[13] = vec4(0.0);  // Previous buffer colour 
			tevSources[15] = v_colour;   // Previous combiner
	)";

	for (int i = 0; i < 6; i++) {
		compileTEV(ret, i, regs);
	}

	return ret;
}

void FragmentGenerator::compileTEV(std::string& shader, int stage, const PICARegs& regs) {
	// Base address for each TEV stage's configuration
	static constexpr std::array<u32, 6> ioBases = {
		InternalRegs::TexEnv0Source, InternalRegs::TexEnv1Source, InternalRegs::TexEnv2Source,
		InternalRegs::TexEnv3Source, InternalRegs::TexEnv4Source, InternalRegs::TexEnv5Source,
	};

	const u32 ioBase = ioBases[stage];
	TexEnvConfig tev(regs[ioBase], regs[ioBase + 1], regs[ioBase + 2], regs[ioBase + 3], regs[ioBase + 4]);
}