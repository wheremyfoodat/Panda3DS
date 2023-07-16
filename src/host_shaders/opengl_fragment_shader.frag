#version 410 core

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

// TEV uniforms
uniform uint u_textureEnvSource[6];
uniform uint u_textureEnvOperand[6];
uniform uint u_textureEnvCombiner[6];
uniform uint u_textureEnvScale[6];

// Depth control uniforms
uniform float u_depthScale;
uniform float u_depthOffset;
uniform bool u_depthmapEnable;

uniform sampler2D u_tex0;
uniform sampler2D u_tex1;
uniform sampler2D u_tex2;
uniform sampler1DArray u_tex_lighting_lut;

uniform uint u_picaRegs[0x200 - 0x48];

// Helper so that the implementation of u_pica_regs can be changed later
uint readPicaReg(uint reg_addr) { return u_picaRegs[reg_addr - 0x48]; }

vec4 tevSources[16];
vec4 tevNextPreviousBuffer;
bool tevUnimplementedSourceFlag = false;

// OpenGL ES 1.1 reference pages for TEVs (this is what the PICA200 implements):
// https://registry.khronos.org/OpenGL-Refpages/es1.1/xhtml/glTexEnv.xml

vec4 tevFetchSource(uint src_id) {
	if (src_id >= 6u && src_id < 13u) {
		tevUnimplementedSourceFlag = true;
	}

	return tevSources[src_id];
}

vec4 tevGetColorAndAlphaSource(int tev_id, int src_id) {
	vec4 result;

	vec4 colorSource = tevFetchSource((u_textureEnvSource[tev_id] >> (src_id * 4)) & 15u);
	vec4 alphaSource = tevFetchSource((u_textureEnvSource[tev_id] >> (src_id * 4 + 16)) & 15u);

	uint colorOperand = (u_textureEnvOperand[tev_id] >> (src_id * 4)) & 15u;
	uint alphaOperand = (u_textureEnvOperand[tev_id] >> (12 + src_id * 4)) & 7u;

	// TODO: figure out what the undocumented values do
	switch (colorOperand) {
		case 0u: result.rgb = colorSource.rgb; break;             // Source color
		case 1u: result.rgb = 1.0 - colorSource.rgb; break;       // One minus source color
		case 2u: result.rgb = vec3(colorSource.a); break;         // Source alpha
		case 3u: result.rgb = vec3(1.0 - colorSource.a); break;   // One minus source alpha
		case 4u: result.rgb = vec3(colorSource.r); break;         // Source red
		case 5u: result.rgb = vec3(1.0 - colorSource.r); break;   // One minus source red
		case 8u: result.rgb = vec3(colorSource.g); break;         // Source green
		case 9u: result.rgb = vec3(1.0 - colorSource.g); break;   // One minus source green
		case 12u: result.rgb = vec3(colorSource.b); break;        // Source blue
		case 13u: result.rgb = vec3(1.0 - colorSource.b); break;  // One minus source blue
		default: break;
	}

	// TODO: figure out what the undocumented values do
	switch (alphaOperand) {
		case 0u: result.a = alphaSource.a; break;        // Source alpha
		case 1u: result.a = 1.0 - alphaSource.a; break;  // One minus source alpha
		case 2u: result.a = alphaSource.r; break;        // Source red
		case 3u: result.a = 1.0 - alphaSource.r; break;  // One minus source red
		case 4u: result.a = alphaSource.g; break;        // Source green
		case 5u: result.a = 1.0 - alphaSource.g; break;  // One minus source green
		case 6u: result.a = alphaSource.b; break;        // Source blue
		case 7u: result.a = 1.0 - alphaSource.b; break;  // One minus source blue
		default: break;
	}

	return result;
}

vec4 tevCalculateCombiner(int tev_id) {
	vec4 source0 = tevGetColorAndAlphaSource(tev_id, 0);
	vec4 source1 = tevGetColorAndAlphaSource(tev_id, 1);
	vec4 source2 = tevGetColorAndAlphaSource(tev_id, 2);

	uint colorCombine = u_textureEnvCombiner[tev_id] & 15u;
	uint alphaCombine = (u_textureEnvCombiner[tev_id] >> 16) & 15u;

	vec4 result = vec4(1.0);

	// TODO: figure out what the undocumented values do
	switch (colorCombine) {
		case 0u: result.rgb = source0.rgb; break;                                            // Replace
		case 1u: result.rgb = source0.rgb * source1.rgb; break;                              // Modulate
		case 2u: result.rgb = min(vec3(1.0), source0.rgb + source1.rgb); break;              // Add
		case 3u: result.rgb = clamp(source0.rgb + source1.rgb - 0.5, 0.0, 1.0); break;       // Add signed
		case 4u: result.rgb = mix(source1.rgb, source0.rgb, source2.rgb); break;             // Interpolate
		case 5u: result.rgb = max(source0.rgb - source1.rgb, 0.0); break;                    // Subtract
		case 6u: result.rgb = vec3(4.0 * dot(source0.rgb - 0.5, source1.rgb - 0.5)); break;  // Dot3 RGB
		case 7u: result = vec4(4.0 * dot(source0.rgb - 0.5, source1.rgb - 0.5)); break;      // Dot3 RGBA
		case 8u: result.rgb = min(source0.rgb * source1.rgb + source2.rgb, 1.0); break;      // Multiply then add
		case 9u: result.rgb = min((source0.rgb + source1.rgb) * source2.rgb, 1.0); break;    // Add then multiply
		default: break;
	}

	if (colorCombine != 7u) {  // The color combiner also writes the alpha channel in the "Dot3 RGBA" mode.
		// TODO: figure out what the undocumented values do
		// TODO: test if the alpha combiner supports all the same modes as the color combiner.
		switch (alphaCombine) {
			case 0u: result.a = source0.a; break;                                      // Replace
			case 1u: result.a = source0.a * source1.a; break;                          // Modulate
			case 2u: result.a = min(1.0, source0.a + source1.a); break;                // Add
			case 3u: result.a = clamp(source0.a + source1.a - 0.5, 0.0, 1.0); break;   // Add signed
			case 4u: result.a = mix(source1.a, source0.a, source2.a); break;           // Interpolate
			case 5u: result.a = max(0.0, source0.a - source1.a); break;                // Subtract
			case 8u: result.a = min(1.0, source0.a * source1.a + source2.a); break;    // Multiply then add
			case 9u: result.a = min(1.0, (source0.a + source1.a) * source2.a); break;  // Add then multiply
			default: break;
		}
	}

	result.rgb *= float(1 << (u_textureEnvScale[tev_id] & 3u));
	result.a *= float(1 << ((u_textureEnvScale[tev_id] >> 16) & 3u));

	return result;
}

#define D0_LUT 0u
#define D1_LUT 1u
#define SP_LUT 2u
#define FR_LUT 3u
#define RB_LUT 4u
#define RG_LUT 5u
#define RR_LUT 6u

float lutLookup(uint lut, uint light, float value) {
	if (lut >= FR_LUT && lut <= RR_LUT) lut -= 1;
	if (lut == SP_LUT) lut = light + 8;
	return texture(u_tex_lighting_lut, vec2(value, lut)).r;
}

vec3 regToColor(uint reg) {
	// Normalization scale to convert from [0...255] to [0.0...1.0]
	const float scale = 1.0 / 255.0;

	return scale * vec3(float(bitfieldExtract(reg, 20, 8)), float(bitfieldExtract(reg, 10, 8)), float(bitfieldExtract(reg, 00, 8)));
}

// Convert an arbitrary-width floating point literal to an f32
float decodeFP(uint hex, uint E, uint M) {
	uint width = M + E + 1u;
	uint bias = 128u - (1u << (E - 1u));
	uint exponent = (hex >> M) & ((1u << E) - 1u);
	uint mantissa = hex & ((1u << M) - 1u);
	uint sign = (hex >> (E + M)) << 31u;

	if ((hex & ((1u << (width - 1u)) - 1u)) != 0) {
		if (exponent == (1u << E) - 1u)
			exponent = 255u;
		else
			exponent += bias;
		hex = sign | (mantissa << (23u - M)) | (exponent << 23u);
	} else {
		hex = sign;
	}

	return uintBitsToFloat(hex);
}

// Implements the following algorthm: https://mathb.in/26766
void calcLighting(out vec4 primary_color, out vec4 secondary_color) {
	// Quaternions describe a transformation from surface-local space to eye space.
	// In surface-local space, by definition (and up to permutation) the normal vector is (0,0,1),
	// the tangent vector is (1,0,0), and the bitangent vector is (0,1,0).
	vec3 normal = normalize(v_normal);
	vec3 tangent = normalize(v_tangent);
	vec3 bitangent = normalize(v_bitangent);
	vec3 view = normalize(v_view);

	uint GPUREG_LIGHTING_ENABLE = readPicaReg(0x008F);
	if (bitfieldExtract(GPUREG_LIGHTING_ENABLE, 0, 1) == 0) {
		primary_color = secondary_color = vec4(1.0);
		return;
	}

	uint GPUREG_LIGHTING_AMBIENT = readPicaReg(0x01C0);
	uint GPUREG_LIGHTING_NUM_LIGHTS = (readPicaReg(0x01C2) & 0x7u) + 1;
	uint GPUREG_LIGHTING_LIGHT_PERMUTATION = readPicaReg(0x01D9);

	primary_color = vec4(vec3(0.0), 1.0);
	secondary_color = vec4(vec3(0.0), 1.0);

	primary_color.rgb += regToColor(GPUREG_LIGHTING_AMBIENT);

	uint GPUREG_LIGHTING_LUTINPUT_ABS = readPicaReg(0x01D0);
	uint GPUREG_LIGHTING_LUTINPUT_SELECT = readPicaReg(0x01D1);
	uint GPUREG_LIGHTING_CONFIG0 = readPicaReg(0x01C3);
	uint GPUREG_LIGHTING_CONFIG1 = readPicaReg(0x01C4);
	uint GPUREG_LIGHTING_LUTINPUT_SCALE = readPicaReg(0x01D2);
	float d[7];

	bool error_unimpl = false;

	for (uint i = 0; i < GPUREG_LIGHTING_NUM_LIGHTS; i++) {
		uint light_id = bitfieldExtract(GPUREG_LIGHTING_LIGHT_PERMUTATION, int(i * 3), 3);

		uint GPUREG_LIGHTi_SPECULAR0 = readPicaReg(0x0140 + 0x10 * light_id);
		uint GPUREG_LIGHTi_SPECULAR1 = readPicaReg(0x0141 + 0x10 * light_id);
		uint GPUREG_LIGHTi_DIFFUSE = readPicaReg(0x0142 + 0x10 * light_id);
		uint GPUREG_LIGHTi_AMBIENT = readPicaReg(0x0143 + 0x10 * light_id);
		uint GPUREG_LIGHTi_VECTOR_LOW = readPicaReg(0x0144 + 0x10 * light_id);
		uint GPUREG_LIGHTi_VECTOR_HIGH = readPicaReg(0x0145 + 0x10 * light_id);
		uint GPUREG_LIGHTi_CONFIG = readPicaReg(0x0149 + 0x10 * light_id);

		vec3 light_vector = normalize(vec3(
			decodeFP(bitfieldExtract(GPUREG_LIGHTi_VECTOR_LOW, 0, 16), 5, 10), decodeFP(bitfieldExtract(GPUREG_LIGHTi_VECTOR_LOW, 16, 16), 5, 10),
			decodeFP(bitfieldExtract(GPUREG_LIGHTi_VECTOR_HIGH, 0, 16), 5, 10)
		));

		// Positional Light
		if (bitfieldExtract(GPUREG_LIGHTi_CONFIG, 0, 1) == 0) error_unimpl = true;

		vec3 half_vector = normalize(normalize(light_vector) + view);

		for (int c = 0; c < 7; c++) {
			if (bitfieldExtract(GPUREG_LIGHTING_CONFIG1, 16 + c, 1) == 0) {
				uint scale_id = bitfieldExtract(GPUREG_LIGHTING_LUTINPUT_SCALE, c * 4, 3);
				float scale = float(1u << scale_id);
				if (scale_id >= 6u) scale /= 256.0;

				uint input_id = bitfieldExtract(GPUREG_LIGHTING_LUTINPUT_SELECT, c * 4, 3);
				if (input_id == 0u)
					d[c] = dot(normal, half_vector);
				else if (input_id == 1u)
					d[c] = dot(view, half_vector);
				else if (input_id == 2u)
					d[c] = dot(normal, view);
				else if (input_id == 3u)
					d[c] = dot(light_vector, normal);
				else if (input_id == 4u) {
					uint GPUREG_LIGHTi_SPOTDIR_LOW = readPicaReg(0x0146 + 0x10 * light_id);
					uint GPUREG_LIGHTi_SPOTDIR_HIGH = readPicaReg(0x0147 + 0x10 * light_id);
					vec3 spot_light_vector = normalize(vec3(
						decodeFP(bitfieldExtract(GPUREG_LIGHTi_SPOTDIR_LOW, 0, 16), 1, 11),
						decodeFP(bitfieldExtract(GPUREG_LIGHTi_SPOTDIR_LOW, 16, 16), 1, 11),
						decodeFP(bitfieldExtract(GPUREG_LIGHTi_SPOTDIR_HIGH, 0, 16), 1, 11)
					));
					d[c] = dot(-light_vector, spot_light_vector);  // -L dot P (aka Spotlight aka SP);
				} else if (input_id == 5u) {
					d[c] = 1.0;  // TODO: cos <greek symbol> (aka CP);
					error_unimpl = true;
				} else {
					d[c] = 1.0;
				}

				d[c] = lutLookup(c, light_id, d[c] * 0.5 + 0.5) * scale;
				if (bitfieldExtract(GPUREG_LIGHTING_LUTINPUT_ABS, 2 * c, 1) != 0u) d[c] = abs(d[c]);
			} else {
				d[c] = 1.0;
			}
		}

		uint lookup_config = bitfieldExtract(GPUREG_LIGHTi_CONFIG, 4, 4);
		if (lookup_config == 0) {
			d[D1_LUT] = 0.0;
			d[FR_LUT] = 0.0;
			d[RG_LUT] = d[RB_LUT] = d[RR_LUT];
		} else if (lookup_config == 1) {
			d[D0_LUT] = 0.0;
			d[D1_LUT] = 0.0;
			d[RG_LUT] = d[RB_LUT] = d[RR_LUT];
		} else if (lookup_config == 2) {
			d[FR_LUT] = 0.0;
			d[SP_LUT] = 0.0;
			d[RG_LUT] = d[RB_LUT] = d[RR_LUT];
		} else if (lookup_config == 3) {
			d[SP_LUT] = 0.0;
			d[RG_LUT] = d[RB_LUT] = d[RR_LUT] = 1.0;
		} else if (lookup_config == 4) {
			d[FR_LUT] = 0.0;
		} else if (lookup_config == 5) {
			d[D1_LUT] = 0.0;
		} else if (lookup_config == 6) {
			d[RG_LUT] = d[RB_LUT] = d[RR_LUT];
		}

		float distance_factor = 1.0;  // a
		float indirect_factor = 1.0;  // fi
		float shadow_factor = 1.0;    // o

		float NdotL = dot(normal, light_vector);  // Li dot N

		// Two sided diffuse
		if (bitfieldExtract(GPUREG_LIGHTi_CONFIG, 1, 1) == 0)
			NdotL = max(0.0, NdotL);
		else
			NdotL = abs(NdotL);

		float light_factor = distance_factor * d[SP_LUT] * indirect_factor * shadow_factor;

		primary_color.rgb += light_factor * (regToColor(GPUREG_LIGHTi_AMBIENT) + regToColor(GPUREG_LIGHTi_DIFFUSE) * NdotL);
		secondary_color.rgb += light_factor * (regToColor(GPUREG_LIGHTi_SPECULAR0) * d[D0_LUT] +
											   regToColor(GPUREG_LIGHTi_SPECULAR1) * d[D1_LUT] * vec3(d[RR_LUT], d[RG_LUT], d[RB_LUT]));
	}
	uint fresnel_output1 = bitfieldExtract(GPUREG_LIGHTING_CONFIG0, 2, 1);
	uint fresnel_output2 = bitfieldExtract(GPUREG_LIGHTING_CONFIG0, 3, 1);

	if (fresnel_output1 == 1u) primary_color.a = d[FR_LUT];
	if (fresnel_output2 == 1u) secondary_color.a = d[FR_LUT];

	if (error_unimpl) {
		secondary_color = primary_color = vec4(1.0, 0., 1.0, 1.0);
	}
}

void main() {
	// TODO: what do invalid sources and disabled textures read as?
	// And what does the "previous combiner" source read initially?
	tevSources[0] = v_colour;  // Primary/vertex color
	calcLighting(tevSources[1], tevSources[2]);

	uint textureConfig = readPicaReg(0x80);
	vec2 tex2UV = (textureConfig & (1u << 13)) != 0u ? v_texcoord1 : v_texcoord2;

	if ((textureConfig & 1u) != 0u) tevSources[3] = texture(u_tex0, v_texcoord0.xy);
	if ((textureConfig & 2u) != 0u) tevSources[4] = texture(u_tex1, v_texcoord1);
	if ((textureConfig & 4u) != 0u) tevSources[5] = texture(u_tex2, tex2UV);
	tevSources[13] = vec4(0.0);  // Previous buffer
	tevSources[15] = vec4(0.0);  // Previous combiner

	tevNextPreviousBuffer = v_textureEnvBufferColor;
	uint textureEnvUpdateBuffer = readPicaReg(0xE0);

	for (int i = 0; i < 6; i++) {
		tevSources[14] = v_textureEnvColor[i];  // Constant color
		tevSources[15] = tevCalculateCombiner(i);
		tevSources[13] = tevNextPreviousBuffer;

		if (i < 4) {
			if ((textureEnvUpdateBuffer & (0x100u << i)) != 0u) {
				tevNextPreviousBuffer.rgb = tevSources[15].rgb;
			}

			if ((textureEnvUpdateBuffer & (0x1000u << i)) != 0u) {
				tevNextPreviousBuffer.a = tevSources[15].a;
			}
		}
	}

	fragColour = tevSources[15];

	if (tevUnimplementedSourceFlag) {
		// fragColour = vec4(1.0, 0.0, 1.0, 1.0);
	}
	// fragColour.rg = texture(u_tex_lighting_lut,vec2(gl_FragCoord.x/200.,float(int(gl_FragCoord.y/2)%24))).rr;

	// Get original depth value by converting from [near, far] = [0, 1] to [-1, 1]
	// We do this by converting to [0, 2] first and subtracting 1 to go to [-1, 1]
	float z_over_w = gl_FragCoord.z * 2.0f - 1.0f;
	float depth = z_over_w * u_depthScale + u_depthOffset;

	if (!u_depthmapEnable)  // Divide z by w if depthmap enable == 0 (ie using W-buffering)
		depth /= gl_FragCoord.w;

	// Write final fragment depth
	gl_FragDepth = depth;

	// Perform alpha test
	uint alphaControl = readPicaReg(0x104);
	if ((alphaControl & 1u) != 0u) {  // Check if alpha test is on
		uint func = (alphaControl >> 4u) & 7u;
		float reference = float((alphaControl >> 8u) & 0xffu) / 255.0;
		float alpha = fragColour.a;

		switch (func) {
			case 0: discard;  // Never pass alpha test
			case 1: break;    // Always pass alpha test
			case 2:           // Pass if equal
				if (alpha != reference) discard;
				break;
			case 3:  // Pass if not equal
				if (alpha == reference) discard;
				break;
			case 4:  // Pass if less than
				if (alpha >= reference) discard;
				break;
			case 5:  // Pass if less than or equal
				if (alpha > reference) discard;
				break;
			case 6:  // Pass if greater than
				if (alpha <= reference) discard;
				break;
			case 7:  // Pass if greater than or equal
				if (alpha < reference) discard;
				break;
		}
	}
}