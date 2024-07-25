#include "PICA/pica_frag_config.hpp"
#include "PICA/regs.hpp"
#include "PICA/shader_gen.hpp"
using namespace PICA;
using namespace PICA::ShaderGen;

static constexpr const char* uniformDefinition = R"(
	struct LightSource {
		vec3 specular0;
		vec3 specular1;
		vec3 diffuse;
		vec3 ambient;
		vec3 position;
		vec3 spotlightDirection;
		float distanceAttenuationBias;
		float distanceAttenuationScale;
	};

	layout(std140) uniform FragmentUniforms {
		int alphaReference;
		float depthScale;
		float depthOffset;

		vec4 constantColors[6];
		vec4 tevBufferColor;
		vec4 clipCoords;

		// Note: We upload this as a u32 and decode on GPU
		uint globalAmbientLight;
		uint inFogColor;
		LightSource lightSources[8];
	};
)";

std::string FragmentGenerator::getDefaultVertexShader() {
	std::string ret = "";

	switch (api) {
		case API::GL: ret += "#version 410 core"; break;
		case API::GLES: ret += "#version 300 es"; break;
		default: break;
	}

	if (api == API::GLES) {
		ret += R"(
			#define USING_GLES 1

			precision mediump int;
			precision mediump float;
		)";
	}

	ret += uniformDefinition;

	ret += R"(
		layout(location = 0) in vec4 a_coords;
		layout(location = 1) in vec4 a_quaternion;
		layout(location = 2) in vec4 a_vertexColour;
		layout(location = 3) in vec2 a_texcoord0;
		layout(location = 4) in vec2 a_texcoord1;
		layout(location = 5) in float a_texcoord0_w;
		layout(location = 6) in vec3 a_view;
		layout(location = 7) in vec2 a_texcoord2;

		out vec4 v_quaternion;
		out vec4 v_colour;
		out vec3 v_texcoord0;
		out vec2 v_texcoord1;
		out vec3 v_view;
		out vec2 v_texcoord2;

	#ifndef USING_GLES
		out float gl_ClipDistance[2];
	#endif

		void main() {
			gl_Position = a_coords;
			vec4 colourAbs = abs(a_vertexColour);
			v_colour = min(colourAbs, vec4(1.f));

			v_texcoord0 = vec3(a_texcoord0.x, 1.0 - a_texcoord0.y, a_texcoord0_w);
			v_texcoord1 = vec2(a_texcoord1.x, 1.0 - a_texcoord1.y);
			v_texcoord2 = vec2(a_texcoord2.x, 1.0 - a_texcoord2.y);
			v_view = a_view;
			v_quaternion = a_quaternion;

		#ifndef USING_GLES
			gl_ClipDistance[0] = -a_coords.z;
			gl_ClipDistance[1] = dot(clipCoords, a_coords);
		#endif
		}
)";

	return ret;
}

std::string FragmentGenerator::generate(const FragmentConfig& config) {
	std::string ret = "";

	switch (api) {
		case API::GL: ret += "#version 410 core"; break;
		case API::GLES: ret += "#version 300 es"; break;
		default: break;
	}

	bool unimplementedFlag = false;
	if (api == API::GLES) {
		ret += R"(
			#define USING_GLES 1

			precision mediump int;
			precision mediump float;
		)";
	}

	// Input and output attributes
	ret += R"(
		in vec4 v_quaternion;
		in vec4 v_colour;
		in vec3 v_texcoord0;
		in vec2 v_texcoord1;
		in vec3 v_view;
		in vec2 v_texcoord2;

		out vec4 fragColor;
		uniform sampler2D u_tex0;
		uniform sampler2D u_tex1;
		uniform sampler2D u_tex2;
		uniform sampler2D u_tex_luts;
	)";

	ret += uniformDefinition;

	if (config.lighting.enable) {
		ret += R"(
			vec3 rotateVec3ByQuaternion(vec3 v, vec4 q) {
				vec3 u = q.xyz;
				float s = q.w;
				return 2.0 * dot(u, v) * u + (s * s - dot(u, u)) * v + 2.0 * s * cross(u, v);
			}

			float lutLookup(uint lut, int index) {
				return texelFetch(u_tex_luts, ivec2(index, int(lut)), 0).r;
			}

			vec3 regToColor(uint reg) {
				return (1.0 / 255.0) * vec3(float((reg >> 20u) & 0xFFu), float((reg >> 10u) & 0xFFu), float(reg & 0xFFu));
			}
		)";
	}

	// Emit main function for fragment shader
	// When not initialized, source 13 is set to vec4(0.0) and 15 is set to the vertex colour
	ret += R"(
		void main() {
			vec4 combinerOutput = v_colour;
			vec4 previousBuffer = vec4(0.0);
			vec4 tevNextPreviousBuffer = tevBufferColor;

			vec4 primaryColor = vec4(0.0);
			vec4 secondaryColor = vec4(0.0);
	)";

	compileLights(ret, config);

	ret += R"(
		vec3 colorOp1 = vec3(0.0);
		vec3 colorOp2 = vec3(0.0);
		vec3 colorOp3 = vec3(0.0);

		float alphaOp1 = 0.0;
		float alphaOp2 = 0.0;
		float alphaOp3 = 0.0;
	)";

	// Get original depth value by converting from [near, far] = [0, 1] to [-1, 1]
	// We do this by converting to [0, 2] first and subtracting 1 to go to [-1, 1]
	ret += R"(
		float z_over_w = gl_FragCoord.z * 2.0f - 1.0f;
		float depth = z_over_w * depthScale + depthOffset;
	)";

	if (!config.outConfig.depthMapEnable) {
		ret += "depth /= gl_FragCoord.w;\n";
	}

	ret += "gl_FragDepth = depth;\n";

	for (int i = 0; i < 6; i++) {
		compileTEV(ret, i, config);
	}

	compileFog(ret, config);

	applyAlphaTest(ret, config);

	ret += "fragColor = combinerOutput;\n}"; // End of main function

	return ret;
}

void FragmentGenerator::compileTEV(std::string& shader, int stage, const PICA::FragmentConfig& config) {
	const u32* tevValues = config.texConfig.tevConfigs.data() + stage * 4;

	// Pass a 0 to constColor here, as it doesn't matter for compilation
	TexEnvConfig tev(tevValues[0], tevValues[1], tevValues[2], 0, tevValues[3]);

	if (!tev.isPassthroughStage()) {
		// Get color operands
		shader += "colorOp1 = ";
		getColorOperand(shader, tev.colorSource1, tev.colorOperand1, stage, config);

		shader += ";\ncolorOp2 = ";
		getColorOperand(shader, tev.colorSource2, tev.colorOperand2, stage, config);

		shader += ";\ncolorOp3 = ";
		getColorOperand(shader, tev.colorSource3, tev.colorOperand3, stage, config);

		shader += ";\nvec3 outputColor" + std::to_string(stage) + " = clamp(";
		getColorOperation(shader, tev.colorOp);
		shader += ", vec3(0.0), vec3(1.0));\n";

		if (tev.colorOp == TexEnvConfig::Operation::Dot3RGBA) {
			// Dot3 RGBA also writes to the alpha component so we don't need to do anything more
			shader += "float outputAlpha" + std::to_string(stage) + " = outputColor" + std::to_string(stage) + ".x;\n";
		} else {
			// Get alpha operands
			shader += "alphaOp1 = ";
			getAlphaOperand(shader, tev.alphaSource1, tev.alphaOperand1, stage, config);

			shader += ";\nalphaOp2 = ";
			getAlphaOperand(shader, tev.alphaSource2, tev.alphaOperand2, stage, config);

			shader += ";\nalphaOp3 = ";
			getAlphaOperand(shader, tev.alphaSource3, tev.alphaOperand3, stage, config);

			shader += ";\nfloat outputAlpha" + std::to_string(stage) + " = clamp(";
			getAlphaOperation(shader, tev.alphaOp);
			// Clamp the alpha value to [0.0, 1.0]
			shader += ", 0.0, 1.0);\n";
		}

		shader += "combinerOutput = vec4(clamp(outputColor" + std::to_string(stage) + " * " + std::to_string(tev.getColorScale()) +
				  ".0, vec3(0.0), vec3(1.0)), clamp(outputAlpha" + std::to_string(stage) + " * " + std::to_string(tev.getAlphaScale()) +
				  ".0, 0.0, 1.0));\n";
	}

	shader += "previousBuffer = tevNextPreviousBuffer;\n\n";

	// Update the "next previous buffer" if necessary
	const u32 textureEnvUpdateBuffer = config.texConfig.texEnvUpdateBuffer;
	if (stage < 4) {
		// Check whether to update rgb
		if ((textureEnvUpdateBuffer & (0x100 << stage))) {
			shader += "tevNextPreviousBuffer.rgb = combinerOutput.rgb;\n";
		}

		// And whether to update alpha
		if ((textureEnvUpdateBuffer & (0x1000u << stage))) {
			shader += "tevNextPreviousBuffer.a = combinerOutput.a;\n";
		}
	}
}

void FragmentGenerator::getColorOperand(std::string& shader, TexEnvConfig::Source source, TexEnvConfig::ColorOperand color, int index, const PICA::FragmentConfig& config) {
	using OperandType = TexEnvConfig::ColorOperand;

	// For inverting operands, add the 1.0 - x subtraction
	if (color == OperandType::OneMinusSourceColor || color == OperandType::OneMinusSourceRed || color == OperandType::OneMinusSourceGreen ||
		color == OperandType::OneMinusSourceBlue || color == OperandType::OneMinusSourceAlpha) {
		shader += "vec3(1.0, 1.0, 1.0) - ";
	}

	switch (color) {
		case OperandType::SourceColor:
		case OperandType::OneMinusSourceColor:
			getSource(shader, source, index, config);
			shader += ".rgb";
			break;

		case OperandType::SourceRed:
		case OperandType::OneMinusSourceRed:
			getSource(shader, source, index, config);
			shader += ".rrr";
			break;

		case OperandType::SourceGreen:
		case OperandType::OneMinusSourceGreen:
			getSource(shader, source, index, config);
			shader += ".ggg";
			break;

		case OperandType::SourceBlue:
		case OperandType::OneMinusSourceBlue:
			getSource(shader, source, index, config);
			shader += ".bbb";
			break;

		case OperandType::SourceAlpha:
		case OperandType::OneMinusSourceAlpha:
			getSource(shader, source, index, config);
			shader += ".aaa";
			break;

		default:
			shader += "vec3(1.0, 1.0, 1.0)";
			Helpers::warn("FragmentGenerator: Invalid TEV color operand");
			break;
	}
}

void FragmentGenerator::getAlphaOperand(std::string& shader, TexEnvConfig::Source source, TexEnvConfig::AlphaOperand color, int index, const PICA::FragmentConfig& config) {
	using OperandType = TexEnvConfig::AlphaOperand;

	// For inverting operands, add the 1.0 - x subtraction
	if (color == OperandType::OneMinusSourceRed || color == OperandType::OneMinusSourceGreen || color == OperandType::OneMinusSourceBlue ||
		color == OperandType::OneMinusSourceAlpha) {
		shader += "1.0 - ";
	}

	switch (color) {
		case OperandType::SourceRed:
		case OperandType::OneMinusSourceRed:
			getSource(shader, source, index, config);
			shader += ".r";
			break;

		case OperandType::SourceGreen:
		case OperandType::OneMinusSourceGreen:
			getSource(shader, source, index, config);
			shader += ".g";
			break;

		case OperandType::SourceBlue:
		case OperandType::OneMinusSourceBlue:
			getSource(shader, source, index, config);
			shader += ".b";
			break;

		case OperandType::SourceAlpha:
		case OperandType::OneMinusSourceAlpha:
			getSource(shader, source, index, config);
			shader += ".a";
			break;

		default:
			shader += "1.0";
			Helpers::warn("FragmentGenerator: Invalid TEV color operand");
			break;
	}
}

void FragmentGenerator::getSource(std::string& shader, TexEnvConfig::Source source, int index, const PICA::FragmentConfig& config) {
	switch (source) {
		case TexEnvConfig::Source::PrimaryColor: shader += "v_colour"; break;
		case TexEnvConfig::Source::Texture0: shader += "texture(u_tex0, v_texcoord0.xy)"; break;
		case TexEnvConfig::Source::Texture1: shader += "texture(u_tex1, v_texcoord1)"; break;
		case TexEnvConfig::Source::Texture2: {
			// If bit 13 in texture config is set then we use the texcoords for texture 1, otherwise for texture 2
			if (Helpers::getBit<13>(config.texConfig.texUnitConfig)) {
				shader += "texture(u_tex2, v_texcoord1)";
			} else {
				shader += "texture(u_tex2, v_texcoord2)";
			}
			break;
		}

		case TexEnvConfig::Source::Previous: shader += "combinerOutput"; break;
		case TexEnvConfig::Source::Constant: shader += "constantColors[" + std::to_string(index) + "]"; break;
		case TexEnvConfig::Source::PreviousBuffer: shader += "previousBuffer"; break;
		
		// Lighting
		case TexEnvConfig::Source::PrimaryFragmentColor: shader += "primaryColor"; break;
		case TexEnvConfig::Source::SecondaryFragmentColor: shader += "secondaryColor"; break;

		default:
			Helpers::warn("Unimplemented TEV source: %d", static_cast<int>(source));
			shader += "vec4(1.0, 1.0, 1.0, 1.0)";
			break;
	}
}

void FragmentGenerator::getColorOperation(std::string& shader, TexEnvConfig::Operation op) {
	switch (op) {
		case TexEnvConfig::Operation::Replace: shader += "colorOp1"; break;
		case TexEnvConfig::Operation::Add: shader += "colorOp1 + colorOp2"; break;
		case TexEnvConfig::Operation::AddSigned: shader += "colorOp1 + colorOp2 - vec3(0.5)"; break;
		case TexEnvConfig::Operation::Subtract: shader += "colorOp1 - colorOp2"; break;
		case TexEnvConfig::Operation::Modulate: shader += "colorOp1 * colorOp2"; break;
		case TexEnvConfig::Operation::Lerp: shader += "mix(colorOp2, colorOp1, colorOp3)"; break;

		case TexEnvConfig::Operation::AddMultiply: shader += "min(colorOp1 + colorOp2, vec3(1.0)) * colorOp3"; break;
		case TexEnvConfig::Operation::MultiplyAdd: shader += "fma(colorOp1, colorOp2, colorOp3)"; break;
		case TexEnvConfig::Operation::Dot3RGB:
		case TexEnvConfig::Operation::Dot3RGBA: shader += "vec3(4.0 * dot(colorOp1 - vec3(0.5), colorOp2 - vec3(0.5)))"; break;
		default:
			Helpers::warn("FragmentGenerator: Unimplemented color op");
			shader += "vec3(1.0)";
			break;
	}
}

void FragmentGenerator::getAlphaOperation(std::string& shader, TexEnvConfig::Operation op) {
	switch (op) {
		case TexEnvConfig::Operation::Replace: shader += "alphaOp1"; break;
		case TexEnvConfig::Operation::Add: shader += "alphaOp1 + alphaOp2"; break;
		case TexEnvConfig::Operation::AddSigned: shader += "alphaOp1 + alphaOp2 - 0.5"; break;
		case TexEnvConfig::Operation::Subtract: shader += "alphaOp1 - alphaOp2"; break;
		case TexEnvConfig::Operation::Modulate: shader += "alphaOp1 * alphaOp2"; break;
		case TexEnvConfig::Operation::Lerp: shader += "mix(alphaOp2, alphaOp1, alphaOp3)"; break;

		case TexEnvConfig::Operation::AddMultiply: shader += "min(alphaOp1 + alphaOp2, 1.0) * alphaOp3"; break;
		case TexEnvConfig::Operation::MultiplyAdd: shader += "fma(alphaOp1, alphaOp2, alphaOp3)"; break;
		default:
			Helpers::warn("FragmentGenerator: Unimplemented alpha op");
			shader += "1.0";
			break;
	}
}

void FragmentGenerator::applyAlphaTest(std::string& shader, const PICA::FragmentConfig& config) {
	const CompareFunction function = config.outConfig.alphaTestFunction;

	// Alpha test disabled
	if (function == CompareFunction::Always) {
		return;
	}

	shader += "int testingAlpha = int(combinerOutput.a * 255.0);\n";
	shader += "if (";
	switch (function) {
		case CompareFunction::Never: shader += "true"; break;
		case CompareFunction::Always: shader += "false"; break;
		case CompareFunction::Equal: shader += "testingAlpha != alphaReference"; break;
		case CompareFunction::NotEqual: shader += "testingAlpha == alphaReference"; break;
		case CompareFunction::Less: shader += "testingAlpha >= alphaReference"; break;
		case CompareFunction::LessOrEqual: shader += "testingAlpha > alphaReference"; break;
		case CompareFunction::Greater: shader += "testingAlpha <= alphaReference"; break;
		case CompareFunction::GreaterOrEqual: shader += "testingAlpha < alphaReference"; break;

		default:
			Helpers::warn("Unimplemented alpha test function");
			shader += "false";
			break;
	}

	shader += ") { discard; }\n";
}

void FragmentGenerator::compileLights(std::string& shader, const PICA::FragmentConfig& config) {
	if (!config.lighting.enable) {
		return;
	}

	// Currently ignore bump mode
	shader += "vec3 normal = rotateVec3ByQuaternion(vec3(0.0, 0.0, 1.0), v_quaternion);\n";
	shader += R"(
		vec4 diffuse_sum = vec4(0.0, 0.0, 0.0, 1.0);
		vec4 specular_sum = vec4(0.0, 0.0, 0.0, 1.0);
		vec3 light_position, light_vector, half_vector, specular0, specular1, reflected_color;

		float light_distance, NdotL, light_factor, geometric_factor, distance_attenuation, distance_att_delta;
		float spotlight_attenuation, specular0_dist, specular1_dist;
		float lut_lookup_result, lut_lookup_delta;
		int lut_lookup_index;
	)";

	uint lightID = 0;

	for (int i = 0; i < config.lighting.lightNum; i++) {
		lightID = config.lighting.lights[i].num; 

		const auto& lightConfig = config.lighting.lights[i];
		shader += "light_position = lightSources[" + std::to_string(lightID) + "].position;\n";

		if (lightConfig.directional) {  // Directional lighting
			shader += "light_vector = light_position;\n";
		} else {  // Positional lighting
			shader += "light_vector = light_position + v_view;\n";
		}

		shader += R"(
			light_distance = length(light_vector);
			light_vector = normalize(light_vector);
			half_vector = light_vector + normalize(v_view);

			distance_attenuation = 1.0;
			NdotL = dot(normal, light_vector);
		)";

		shader += lightConfig.twoSidedDiffuse ? "NdotL = abs(NdotL);\n" : "NdotL = max(NdotL, 0.0);\n";

		if (lightConfig.geometricFactor0 || lightConfig.geometricFactor1) {
			shader += R"(
				geometric_factor = dot(half_vector, half_vector);
				geometric_factor = (geometric_factor == 0.0) ? 0.0 : min(NdotL / geometric_factor, 1.0);
			)";
		}

		if (lightConfig.distanceAttenuationEnable) {
			shader += "distance_att_delta = clamp(light_distance * lightSources[" + std::to_string(lightID) +
					  "].distanceAttenuationScale + lightSources[" + std::to_string(lightID) + "].distanceAttenuationBias, 0.0, 1.0);\n";

			shader += "distance_attenuation = lutLookup(" + std::to_string(16 + lightID) +
					  ", int(clamp(floor(distance_att_delta * 256.0), 0.0, 255.0)));\n";
		}

		compileLUTLookup(shader, config, i, spotlightLutIndex);
		shader += "spotlight_attenuation = lut_lookup_result;\n";

		compileLUTLookup(shader, config, i, PICA::Lights::LUT_D0);
		shader += "specular0_dist = lut_lookup_result;\n";

		compileLUTLookup(shader, config, i, PICA::Lights::LUT_D1);
		shader += "specular1_dist = lut_lookup_result;\n";

		compileLUTLookup(shader, config, i, PICA::Lights::LUT_RR);
		shader += "reflected_color.r = lut_lookup_result;\n";

		if (isSamplerEnabled(config.lighting.config, PICA::Lights::LUT_RG)) {
			compileLUTLookup(shader, config, i, PICA::Lights::LUT_RG);
			shader += "reflected_color.g = lut_lookup_result;\n";
		} else {
			shader += "reflected_color.g = reflected_color.r;\n";
		}

		if (isSamplerEnabled(config.lighting.config, PICA::Lights::LUT_RB)) {
			compileLUTLookup(shader, config, i, PICA::Lights::LUT_RB);
			shader += "reflected_color.b = lut_lookup_result;\n";
		} else {
			shader += "reflected_color.b = reflected_color.r;\n";
		}

		shader += "specular0 = lightSources[" + std::to_string(lightID) + "].specular0 * specular0_dist;\n";
		if (lightConfig.geometricFactor0) {
			shader += "specular0 *= geometric_factor;\n";
		}

		shader += "specular1 = lightSources[" + std::to_string(lightID) + "].specular1 * specular1_dist * reflected_color;\n";
		if (lightConfig.geometricFactor1) {
			shader += "specular1 *= geometric_factor;\n";
		}

		shader += "light_factor = distance_attenuation * spotlight_attenuation;\n";

		if (config.lighting.clampHighlights) {
			shader += "specular_sum.rgb += light_factor * (NdotL == 0.0 ? 0.0 : 1.0) * (specular0 + specular1);\n";
		} else {
			shader += "specular_sum.rgb += light_factor * (specular0 + specular1);\n";
		}

		shader += "diffuse_sum.rgb += light_factor * (lightSources[" + std::to_string(lightID) + "].ambient + lightSources[" +
				  std::to_string(lightID) + "].diffuse * NdotL);\n";
	}

	if (config.lighting.enablePrimaryAlpha || config.lighting.enableSecondaryAlpha) {
		compileLUTLookup(shader, config, config.lighting.lightNum - 1, PICA::Lights::LUT_FR);
		shader += "float fresnel_factor = lut_lookup_result;\n";
	}

	if (config.lighting.enablePrimaryAlpha) {
		shader += "diffuse_sum.a = fresnel_factor;\n";
	}

	if (config.lighting.enableSecondaryAlpha) {
		shader += "specular_sum.a = fresnel_factor;\n";
	}

	shader += R"(
		vec4 global_ambient = vec4(regToColor(globalAmbientLight), 1.0);

		primaryColor = clamp(global_ambient + diffuse_sum, vec4(0.0), vec4(1.0));
		secondaryColor = clamp(specular_sum, vec4(0.0), vec4(1.0));
	)";
}

bool FragmentGenerator::isSamplerEnabled(u32 environmentID, u32 lutID) {
	static constexpr bool samplerEnabled[9 * 7] = {
		// D0     D1     SP     FR     RB     RG     RR
		true,  false, true,  false, false, false, true,   // Configuration 0: D0, SP, RR
		false, false, true,  true,  false, false, true,   // Configuration 1: FR, SP, RR
		true,  true,  false, false, false, false, true,   // Configuration 2: D0, D1, RR
		true,  true,  false, true,  false, false, false,  // Configuration 3: D0, D1, FR
		true,  true,  true,  false, true,  true,  true,   // Configuration 4: All except for FR
		true,  false, true,  true,  true,  true,  true,   // Configuration 5: All except for D1
		true,  true,  true,  true,  false, false, true,   // Configuration 6: All except for RB and RG
		false, false, false, false, false, false, false,  // Configuration 7: Unused
	 	true,  true,  true,  true,  true,  true,  true,   // Configuration 8: All
	};

	return samplerEnabled[environmentID * 7 + lutID];
}

void FragmentGenerator::compileLUTLookup(std::string& shader, const PICA::FragmentConfig& config, u32 lightIndex, u32 lutID) {
	const LightingLUTConfig& lut = config.lighting.luts[lutID];
	uint lightID = config.lighting.lights[lightIndex].num;
	uint lutIndex = 0;
	bool lutEnabled = false;

	if (lutID == spotlightLutIndex) {
		// These are the spotlight attenuation LUTs
		lutIndex = 8u + lightID;
		lutEnabled = config.lighting.lights[lightIndex].spotAttenuationEnable;
	} else if (lutID <= 6) {
		lutIndex = lutID;
		lutEnabled = lut.enable;
	} else {
		Helpers::warn("Shadergen: Unimplemented LUT value");
	}

	const bool samplerEnabled = isSamplerEnabled(config.lighting.config, lutID);

	if (!samplerEnabled || !lutEnabled) {
		shader += "lut_lookup_result = 1.0;\n";
		return;
	}

	uint scale = lut.scale;
	uint inputID = lut.type;
	bool absEnabled = lut.absInput;
	
	switch (inputID) {
		case 0: shader += "lut_lookup_delta = dot(normal, normalize(half_vector));\n"; break;
		case 1: shader += "lut_lookup_delta = dot(normalize(v_view), normalize(half_vector));\n"; break;
		case 2: shader += "lut_lookup_delta = dot(normal, normalize(v_view));\n"; break;
		case 3: shader += "lut_lookup_delta = dot(normal, light_vector);\n"; break;
		case 4: shader += "lut_lookup_delta = dot(light_vector, lightSources[" + std ::to_string(lightID) + "].spotlightDirection);\n"; break;

		default:
			Helpers::warn("Shadergen: Unimplemented LUT select");
			shader += "lut_lookup_delta = 1.0;\n";
			break;
	}

	static constexpr float scales[] = {1.0f, 2.0f, 4.0f, 8.0f, 0.0f, 0.0f, 0.25f, 0.5f};

	if (absEnabled) {
		bool twoSidedDiffuse = config.lighting.lights[lightIndex].twoSidedDiffuse;
		shader += twoSidedDiffuse ? "lut_lookup_delta = abs(lut_lookup_delta);\n" : "lut_lookup_delta = max(lut_lookup_delta, 0.0);\n";
		shader += "lut_lookup_result = lutLookup(" + std::to_string(lutIndex) + ", int(clamp(floor(lut_lookup_delta * 256.0), 0.0, 255.0)));\n";
		if (scale != 0) {
			shader += "lut_lookup_result *= " + std::to_string(scales[scale]) + ";\n";
		}
	} else {
		// Range is [-1, 1] so we need to map it to [0, 1]
		shader += "lut_lookup_index = int(clamp(floor(lut_lookup_delta * 128.0), -128.f, 127.f));\n";
		shader += "if (lut_lookup_index < 0) lut_lookup_index += 256;\n";
		shader += "lut_lookup_result = lutLookup(" + std::to_string(lutIndex) + ", lut_lookup_index);\n";
		if (scale != 0) {
			shader += "lut_lookup_result *= " + std::to_string(scales[scale]) + ";\n";
		}
	}
}

void FragmentGenerator::compileFog(std::string& shader, const PICA::FragmentConfig& config) {
	if (config.fogConfig.mode != FogMode::Fog) {
		return;
	}

	if (config.fogConfig.flipDepth) {
		shader += "float fog_index = (1.0 - depth) * 128.0;\n";
	} else {
		shader += "float fog_index = depth * 128.0;\n";
	}

	shader += "float clamped_index = clamp(floor(fog_index), 0.0, 127.0);";
	shader += "float delta = fog_index - clamped_index;";
	shader += "vec3 fog_color = (1.0 / 255.0) * vec3(float(inFogColor & 0xffu), float((inFogColor >> 8u) & 0xffu), float((inFogColor >> 16u) & 0xffu));";
	shader += "vec2 value = texelFetch(u_tex_luts, ivec2(int(clamped_index), 24), 0).rg;"; // fog LUT is past the light LUTs
	shader += "float fog_factor = clamp(value.r + value.g * delta, 0.0, 1.0);";
	shader += "combinerOutput.rgb = mix(fog_color, combinerOutput.rgb, fog_factor);";
}
