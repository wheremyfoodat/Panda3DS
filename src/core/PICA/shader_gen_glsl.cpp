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

	ret += R"(
		vec3 colorOperand1 = vec3(0.0);
		vec3 colorOperand2 = vec3(0.0);
		vec3 colorOperand3 = vec3(0.0);

		float alphaOperand1 = 0.0;
		float alphaOperand2 = 0.0;
		float alphaOperand3 = 0.0;
	)";

	for (int i = 0; i < 6; i++) {
		compileTEV(ret, i, regs);
	}

	ret += "}"; // End of main function
	ret += "\n\n\n\n\n\n\n\n\n\n\n\n\n";

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

	if (!tev.isPassthroughStage()) {
		// Get color operands
		shader += "colorOp1 = ";
		getColorOperand(shader, tev.colorSource1, tev.colorOperand1, stage);

		shader += ";\ncolorOp2 = ";
		getColorOperand(shader, tev.colorSource2, tev.colorOperand2, stage);

		shader += ";\ncolorOp3 = ";
		getColorOperand(shader, tev.colorSource3, tev.colorOperand3, stage);

		shader += ";\nvec3 outputColor" + std::to_string(stage) + " = vec3(1.0)";
		shader += ";\n";

		if (tev.colorOp == TexEnvConfig::Operation::Dot3RGBA) {
			// Dot3 RGBA also writes to the alpha component so we don't need to do anything more
			shader += "float outputAlpha" + std::to_string(stage) + " = colorOutput" + std::to_string(stage) + ".x;\n";
		} else {
			// Get alpha operands
			shader += "alphaOp1 = ";
			getAlphaOperand(shader, tev.alphaSource1, tev.alphaOperand1, stage);

			shader += ";\nalphaOp2 = ";
			getAlphaOperand(shader, tev.alphaSource2, tev.alphaOperand2, stage);

			shader += ";\nalphaOp3 = ";
			getAlphaOperand(shader, tev.alphaSource3, tev.alphaOperand3, stage);

			shader += ";\nvec3 outputAlpha" + std::to_string(stage) + " = 1.0";
			shader += ";\n";
		}
	}
}

void FragmentGenerator::getColorOperand(std::string& shader, TexEnvConfig::Source source, TexEnvConfig::ColorOperand color, int index) {
	using OperandType = TexEnvConfig::ColorOperand;

	// For inverting operands, add the 1.0 - x subtraction
	if (color == OperandType::OneMinusSourceColor || color == OperandType::OneMinusSourceRed || color == OperandType::OneMinusSourceGreen ||
		color == OperandType::OneMinusSourceBlue || color == OperandType::OneMinusSourceAlpha) {
		shader += "vec3(1.0, 1.0, 1.0) - ";
	}

	switch (color) {
		case OperandType::SourceColor:
		case OperandType::OneMinusSourceColor:
			getSource(shader, source, index);
			shader += ".rgb";
			break;

		case OperandType::SourceRed:
		case OperandType::OneMinusSourceRed:
			getSource(shader, source, index);
			shader += ".rrr";
			break;

		case OperandType::SourceGreen:
		case OperandType::OneMinusSourceGreen:
			getSource(shader, source, index);
			shader += ".ggg";
			break;

		case OperandType::SourceBlue:
		case OperandType::OneMinusSourceBlue:
			getSource(shader, source, index);
			shader += ".bbb";
			break;

		case OperandType::SourceAlpha:
		case OperandType::OneMinusSourceAlpha:
			getSource(shader, source, index);
			shader += ".aaa";
			break;

		default:
			shader += "vec3(1.0, 1.0, 1.0)";
			Helpers::warn("FragmentGenerator: Invalid TEV color operand");
			break;
	}
}

void FragmentGenerator::getAlphaOperand(std::string& shader, TexEnvConfig::Source source, TexEnvConfig::AlphaOperand color, int index) {
	using OperandType = TexEnvConfig::AlphaOperand;

	// For inverting operands, add the 1.0 - x subtraction
	if (color == OperandType::OneMinusSourceRed || color == OperandType::OneMinusSourceGreen || color == OperandType::OneMinusSourceBlue ||
		color == OperandType::OneMinusSourceAlpha) {
		shader += "1.0 - ";
	}

	switch (color) {
		case OperandType::SourceRed:
		case OperandType::OneMinusSourceRed:
			getSource(shader, source, index);
			shader += ".r";
			break;

		case OperandType::SourceGreen:
		case OperandType::OneMinusSourceGreen:
			getSource(shader, source, index);
			shader += ".g";
			break;

		case OperandType::SourceBlue:
		case OperandType::OneMinusSourceBlue:
			getSource(shader, source, index);
			shader += ".b";
			break;

		case OperandType::SourceAlpha:
		case OperandType::OneMinusSourceAlpha:
			getSource(shader, source, index);
			shader += ".a";
			break;

		default:
			shader += "1.0";
			Helpers::warn("FragmentGenerator: Invalid TEV color operand");
			break;
	}
}

void FragmentGenerator::getSource(std::string& shader, TexEnvConfig::Source source, int index) {
	switch (source) {
		case TexEnvConfig::Source::PrimaryColor: shader += "v_colour"; break;

		default:
			Helpers::warn("Unimplemented TEV source: %d", static_cast<int>(source));
			shader += "vec4(1.0, 1.0, 1.0, 1.0)";
			break;
	}
}