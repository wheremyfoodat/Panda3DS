#include "renderer_gl/renderer_gl.hpp"
#include "PICA/float_types.hpp"
#include "PICA/gpu.hpp"
#include "PICA/regs.hpp"

using namespace Floats;
using namespace Helpers;
using namespace PICA;

const char* vertexShader = R"(
	#version 410 core
	
	layout (location = 0) in vec4  a_coords;
	layout (location = 1) in vec4  a_quaternion;
	layout (location = 2) in vec4  a_vertexColour;
	layout (location = 3) in vec2  a_texcoord0;
	layout (location = 4) in vec2  a_texcoord1;
	layout (location = 5) in float a_texcoord0_w;
	layout (location = 6) in vec3  a_view;
	layout (location = 7) in vec2  a_texcoord2;

	out vec3 v_normal;
	out vec3 v_tangent;
	out vec3 v_bitangent;
	out vec4 v_colour;
	out vec3 v_texcoord0;
	out vec2 v_texcoord1;
	out vec3 v_view;
	out vec2 v_texcoord2;
	flat out vec4 v_textureEnvColor[6];
	flat out vec4 v_textureEnvBufferColor;

	out float gl_ClipDistance[2];

	// TEV uniforms
	uniform uint u_textureEnvColor[6];
	uniform uint u_picaRegs[0x200 - 0x48];

	// Helper so that the implementation of u_pica_regs can be changed later
	uint readPicaReg(uint reg_addr){
		return u_picaRegs[reg_addr - 0x48];
	}

	vec4 abgr8888ToVec4(uint abgr) {
		const float scale = 1.0 / 255.0;

		return scale * vec4(
			float(abgr & 0xffu),
			float((abgr >> 8) & 0xffu),
			float((abgr >> 16) & 0xffu),
			float(abgr >> 24)
		);
	}

	vec3 rotateVec3ByQuaternion(vec3 v, vec4 q){
		vec3 u = q.xyz;
		float s = q.w;
		return 2.0 * dot(u, v) * u + (s * s - dot(u, u))* v  + 2.0 * s * cross(u, v);
	}

	// Convert an arbitrary-width floating point literal to an f32
	float decodeFP(uint hex, uint E, uint M){
		uint width = M + E + 1u;
		uint bias = 128u - (1u << (E - 1u));
		uint exponent = (hex >> M) & ((1u << E) - 1u);
		uint mantissa = hex & ((1u << M) - 1u);
		uint sign = (hex >> (E + M)) << 31u;

		if ((hex & ((1u << (width - 1u)) - 1u)) != 0) {
			if (exponent == (1u << E) - 1u) exponent = 255u;
			else exponent += bias;
			hex = sign | (mantissa << (23u - M)) | (exponent << 23u);
		} else {
			hex = sign;
		}

        return uintBitsToFloat(hex);
	}

	void main() {
		gl_Position = a_coords;
		v_colour = a_vertexColour;

		// Flip y axis of UVs because OpenGL uses an inverted y for texture sampling compared to the PICA
		v_texcoord0 = vec3(a_texcoord0.x, 1.0 - a_texcoord0.y, a_texcoord0_w);
		v_texcoord1 = vec2(a_texcoord1.x, 1.0 - a_texcoord1.y);
		v_texcoord2 = vec2(a_texcoord2.x, 1.0 - a_texcoord2.y);
		v_view = a_view; 

		v_normal    = normalize(rotateVec3ByQuaternion(vec3(0.0, 0.0, 1.0), a_quaternion));
		v_tangent   = normalize(rotateVec3ByQuaternion(vec3(1.0, 0.0, 0.0), a_quaternion));
		v_bitangent = normalize(rotateVec3ByQuaternion(vec3(0.0, 1.0, 0.0), a_quaternion));

		for (int i = 0; i < 6; i++) {
			v_textureEnvColor[i] = abgr8888ToVec4(u_textureEnvColor[i]);
		}

		v_textureEnvBufferColor = abgr8888ToVec4(readPicaReg(0xFD));

		// Parse clipping plane registers
		// The plane registers describe a clipping plane in the form of Ax + By + Cz + D = 0 
		// With n = (A, B, C) being the normal vector and D being the origin point distance
		// Therefore, for the second clipping plane, we can just pass the dot product of the clip vector and the input coordinates to gl_ClipDistance[1]
		vec4 clipData = vec4(
			decodeFP(readPicaReg(0x48) & 0xffffffu, 7, 16),
			decodeFP(readPicaReg(0x49) & 0xffffffu, 7, 16),
			decodeFP(readPicaReg(0x4A) & 0xffffffu, 7, 16),
			decodeFP(readPicaReg(0x4B) & 0xffffffu, 7, 16)
		);

		// There's also another, always-on clipping plane based on vertex z
		gl_ClipDistance[0] = -a_coords.z;
		gl_ClipDistance[1] = dot(clipData, a_coords);
	}
)";

const char* fragmentShader = R"(
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
	uint readPicaReg(uint reg_addr){
		return u_picaRegs[reg_addr - 0x48];
	}

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
			case  0u: result.rgb = colorSource.rgb; break;            // Source color
			case  1u: result.rgb = 1.0 - colorSource.rgb; break;      // One minus source color
			case  2u: result.rgb = vec3(colorSource.a); break;        // Source alpha
			case  3u: result.rgb = vec3(1.0 - colorSource.a); break;  // One minus source alpha
			case  4u: result.rgb = vec3(colorSource.r); break;        // Source red
			case  5u: result.rgb = vec3(1.0 - colorSource.r); break;  // One minus source red
			case  8u: result.rgb = vec3(colorSource.g); break;        // Source green
			case  9u: result.rgb = vec3(1.0 - colorSource.g); break;  // One minus source green
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
			case 0u: result.rgb = source0.rgb; break;                                       // Replace
			case 1u: result.rgb = source0.rgb * source1.rgb; break;                         // Modulate
			case 2u: result.rgb = min(vec3(1.0), source0.rgb + source1.rgb); break;         // Add
			case 3u: result.rgb = clamp(source0.rgb + source1.rgb - 0.5, 0.0, 1.0); break;  // Add signed
			case 4u: result.rgb = mix(source1.rgb, source0.rgb, source2.rgb); break;        // Interpolate
			case 5u: result.rgb = max(source0.rgb - source1.rgb, 0.0); break;               // Subtract
			case 6u: result.rgb = vec3(4.0 * dot(source0.rgb - 0.5 , source1.rgb - 0.5)); break;  // Dot3 RGB
			case 7u: result     = vec4(4.0 * dot(source0.rgb - 0.5 , source1.rgb - 0.5)); break;  // Dot3 RGBA
			case 8u: result.rgb = min(source0.rgb * source1.rgb + source2.rgb, 1.0); break;       // Multiply then add
			case 9u: result.rgb = min((source0.rgb + source1.rgb) * source2.rgb, 1.0); break;     // Add then multiply
			default: break;
		}

		if (colorCombine != 7u) { // The color combiner also writes the alpha channel in the "Dot3 RGBA" mode.
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
		result.a   *= float(1 << ((u_textureEnvScale[tev_id] >> 16) & 3u));

		return result;
	}

	#define D0_LUT 0u
	#define D1_LUT 1u
	#define SP_LUT 2u
	#define FR_LUT 3u
	#define RB_LUT 4u
	#define RG_LUT 5u
	#define RR_LUT 6u

	float lutLookup(uint lut, uint light, float value){
		if (lut >= FR_LUT && lut <= RR_LUT)
			lut -= 1;
		if (lut==SP_LUT)
			lut = light + 8;
		return texture(u_tex_lighting_lut, vec2(value, lut)).r; 
	}

	vec3 regToColor(uint reg) {
		// Normalization scale to convert from [0...255] to [0.0...1.0]
		const float scale = 1.0 / 255.0;

		return scale * vec3(
			float(bitfieldExtract(reg, 20, 8)),
			float(bitfieldExtract(reg, 10, 8)),
			float(bitfieldExtract(reg, 00, 8))
		);
	}

	// Convert an arbitrary-width floating point literal to an f32
	float decodeFP(uint hex, uint E, uint M){
		uint width = M + E + 1u;
		uint bias = 128u - (1u << (E - 1u));
		uint exponent = (hex >> M) & ((1u << E) - 1u);
		uint mantissa = hex & ((1u << M) - 1u);
		uint sign = (hex >> (E + M)) << 31u;

		if ((hex & ((1u << (width - 1u)) - 1u)) != 0) {
			if (exponent == (1u << E) - 1u) exponent = 255u;
			else exponent += bias;
			hex = sign | (mantissa << (23u - M)) | (exponent << 23u);
		} else {
			hex = sign;
		}

        return uintBitsToFloat(hex);
	}

	// Implements the following algorthm: https://mathb.in/26766
	void calcLighting(out vec4 primary_color, out vec4 secondary_color){
		// Quaternions describe a transformation from surface-local space to eye space.
		// In surface-local space, by definition (and up to permutation) the normal vector is (0,0,1),
		// the tangent vector is (1,0,0), and the bitangent vector is (0,1,0).
		vec3 normal    = normalize(v_normal   );
		vec3 tangent   = normalize(v_tangent  );
		vec3 bitangent = normalize(v_bitangent);
		vec3 view = normalize(v_view);

		uint GPUREG_LIGHTING_ENABLE  = readPicaReg(0x008F);
		if (bitfieldExtract(GPUREG_LIGHTING_ENABLE, 0, 1) == 0){
			primary_color = secondary_color = vec4(1.0);
			return;
		}

		uint GPUREG_LIGHTING_AMBIENT = readPicaReg(0x01C0);
		uint GPUREG_LIGHTING_NUM_LIGHTS = (readPicaReg(0x01C2) & 0x7u) +1;
		uint GPUREG_LIGHTING_LIGHT_PERMUTATION = readPicaReg(0x01D9);

		primary_color   = vec4(vec3(0.0),1.0);
		secondary_color = vec4(vec3(0.0),1.0);

		primary_color.rgb += regToColor(GPUREG_LIGHTING_AMBIENT);

		uint GPUREG_LIGHTING_LUTINPUT_ABS = readPicaReg(0x01D0);
		uint GPUREG_LIGHTING_LUTINPUT_SELECT = readPicaReg(0x01D1);
		uint GPUREG_LIGHTING_CONFIG0 = readPicaReg(0x01C3);
		uint GPUREG_LIGHTING_CONFIG1 = readPicaReg(0x01C4);
		uint GPUREG_LIGHTING_LUTINPUT_SCALE =  readPicaReg(0x01D2);
		float d[7];

		bool error_unimpl = false;

		for (uint i = 0; i < GPUREG_LIGHTING_NUM_LIGHTS; i++) {
			uint light_id = bitfieldExtract(GPUREG_LIGHTING_LIGHT_PERMUTATION,int(i*3),3);
		
			uint GPUREG_LIGHTi_SPECULAR0 = readPicaReg(0x0140 + 0x10 * light_id);
			uint GPUREG_LIGHTi_SPECULAR1 = readPicaReg(0x0141 + 0x10 * light_id);
			uint GPUREG_LIGHTi_DIFFUSE = readPicaReg(0x0142 + 0x10 * light_id);
			uint GPUREG_LIGHTi_AMBIENT = readPicaReg(0x0143 + 0x10 * light_id);
			uint GPUREG_LIGHTi_VECTOR_LOW = readPicaReg(0x0144 + 0x10 * light_id);
			uint GPUREG_LIGHTi_VECTOR_HIGH= readPicaReg(0x0145 + 0x10 * light_id);
			uint GPUREG_LIGHTi_CONFIG = readPicaReg(0x0149 + 0x10 * light_id);

			vec3 light_vector = normalize(vec3(
				decodeFP(bitfieldExtract(GPUREG_LIGHTi_VECTOR_LOW, 0, 16), 5, 10),
				decodeFP(bitfieldExtract(GPUREG_LIGHTi_VECTOR_LOW, 16, 16), 5, 10),
				decodeFP(bitfieldExtract(GPUREG_LIGHTi_VECTOR_HIGH, 0, 16), 5, 10)
			));

			// Positional Light
			if (bitfieldExtract(GPUREG_LIGHTi_CONFIG, 0, 1) == 0)
				error_unimpl = true;

			vec3 half_vector = normalize(normalize(light_vector) + view);

			for (int c = 0; c < 7; c++) {
				if (bitfieldExtract(GPUREG_LIGHTING_CONFIG1, 16 + c, 1) == 0){
					uint scale_id = bitfieldExtract(GPUREG_LIGHTING_LUTINPUT_SCALE, c * 4, 3);
					float scale = float(1u << scale_id);
					if (scale_id >= 6u)
						scale/=256.0;

					uint input_id = bitfieldExtract(GPUREG_LIGHTING_LUTINPUT_SELECT, c * 4, 3);
					if (input_id == 0u) d[c] = dot(normal,half_vector);
					else if (input_id == 1u) d[c] = dot(view,half_vector);
					else if (input_id == 2u) d[c] = dot(normal,view);
					else if (input_id == 3u) d[c] = dot(light_vector,normal);
					else if (input_id == 4u){
						uint GPUREG_LIGHTi_SPOTDIR_LOW = readPicaReg(0x0146 + 0x10 * light_id);
						uint GPUREG_LIGHTi_SPOTDIR_HIGH= readPicaReg(0x0147 + 0x10 * light_id);
						vec3 spot_light_vector = normalize(vec3(
							decodeFP(bitfieldExtract(GPUREG_LIGHTi_SPOTDIR_LOW, 0, 16), 1, 11),
							decodeFP(bitfieldExtract(GPUREG_LIGHTi_SPOTDIR_LOW, 16, 16), 1, 11),
							decodeFP(bitfieldExtract(GPUREG_LIGHTi_SPOTDIR_HIGH, 0, 16), 1, 11)
						));
						d[c] = dot(-light_vector, spot_light_vector); // -L dot P (aka Spotlight aka SP);
					} else if (input_id == 5u) {
						d[c] = 1.0; // TODO: cos <greek symbol> (aka CP);
						error_unimpl = true;
					} else {
						d[c] = 1.0;
					}

					d[c] = lutLookup(c, light_id, d[c] * 0.5 + 0.5) * scale;
					if (bitfieldExtract(GPUREG_LIGHTING_LUTINPUT_ABS, 2 * c, 1) != 0u) 
						d[c] = abs(d[c]);
				} else {
					d[c] = 1.0;
				}
			}
			
			uint lookup_config = bitfieldExtract(GPUREG_LIGHTi_CONFIG,4,4);
			if (lookup_config == 0) {
				d[D1_LUT] = 0.0;
				d[FR_LUT] = 0.0;
				d[RG_LUT]= d[RB_LUT] = d[RR_LUT];
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
				d[RG_LUT]= d[RB_LUT] = d[RR_LUT] = 1.0;
			} else if (lookup_config == 4) {
				d[FR_LUT] = 0.0;
			} else if (lookup_config == 5) {
				d[D1_LUT] = 0.0;
			} else if (lookup_config == 6) {
				d[RG_LUT] = d[RB_LUT] = d[RR_LUT];
			}

			float distance_factor = 1.0; // a
			float indirect_factor = 1.0; // fi
			float shadow_factor = 1.0;   // o

			float NdotL = dot(normal, light_vector); //Li dot N

			// Two sided diffuse
			if (bitfieldExtract(GPUREG_LIGHTi_CONFIG, 1, 1) == 0) NdotL = max(0.0, NdotL);
			else NdotL = abs(NdotL);

			float light_factor =  distance_factor*d[SP_LUT]*indirect_factor*shadow_factor;

			primary_color.rgb   += light_factor * (regToColor(GPUREG_LIGHTi_AMBIENT) + regToColor(GPUREG_LIGHTi_DIFFUSE)*NdotL);
			secondary_color.rgb += light_factor * (
									 regToColor(GPUREG_LIGHTi_SPECULAR0) * d[D0_LUT] +
									 regToColor(GPUREG_LIGHTi_SPECULAR1) * d[D1_LUT] * vec3(d[RR_LUT], d[RG_LUT], d[RB_LUT])
									);
		}	
		uint fresnel_output1 = bitfieldExtract(GPUREG_LIGHTING_CONFIG0, 2, 1);
		uint fresnel_output2 = bitfieldExtract(GPUREG_LIGHTING_CONFIG0, 3, 1);

		if (fresnel_output1 == 1u) primary_color.a = d[FR_LUT];
		if (fresnel_output2 == 1u) secondary_color.a = d[FR_LUT];

		if (error_unimpl) {
			secondary_color = primary_color = vec4(1.0,0.,1.0,1.0);
		}
	}

	void main() {
		// TODO: what do invalid sources and disabled textures read as?
		// And what does the "previous combiner" source read initially?
		tevSources[0] = v_colour; // Primary/vertex color
		calcLighting(tevSources[1],tevSources[2]);

		uint textureConfig = readPicaReg(0x80);
		vec2 tex2UV = (textureConfig & (1u << 13)) != 0u ? v_texcoord1 : v_texcoord2;

		if ((textureConfig & 1u) != 0u) tevSources[3] = texture(u_tex0, v_texcoord0.xy);
		if ((textureConfig & 2u) != 0u) tevSources[4] = texture(u_tex1, v_texcoord1);
		if ((textureConfig & 4u) != 0u) tevSources[5] = texture(u_tex2, tex2UV);
		tevSources[13] = vec4(0.0); // Previous buffer
		tevSources[15] = vec4(0.0); // Previous combiner

		tevNextPreviousBuffer = v_textureEnvBufferColor;
		uint textureEnvUpdateBuffer = readPicaReg(0xE0);

		for (int i = 0; i < 6; i++) {
			tevSources[14] = v_textureEnvColor[i]; // Constant color
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

		if (!u_depthmapEnable) // Divide z by w if depthmap enable == 0 (ie using W-buffering)
			depth /= gl_FragCoord.w;

		// Write final fragment depth
		gl_FragDepth = depth;

		// Perform alpha test
		uint alphaControl = readPicaReg(0x104);
		if ((alphaControl & 1u) != 0u) { // Check if alpha test is on
			uint func = (alphaControl >> 4u) & 7u;
			float reference = float((alphaControl >> 8u) & 0xffu) / 255.0;
			float alpha = fragColour.a;

			switch (func) {
				case 0: discard; // Never pass alpha test
				case 1: break;          // Always pass alpha test
				case 2:                 // Pass if equal
					if (alpha != reference)
						discard;
					break;
				case 3:                 // Pass if not equal
					if (alpha == reference)
						discard;
					break;
				case 4:                 // Pass if less than
					if (alpha >= reference)
						discard;
					break;
				case 5:                 // Pass if less than or equal
					if (alpha > reference)
						discard;
					break;
				case 6:                 // Pass if greater than
					if (alpha <= reference)
						discard;
					break;
				case 7:                 // Pass if greater than or equal
					if (alpha < reference)
						discard;
					break;
			}
		}
	}
)";

const char* displayVertexShader = R"(
	#version 410 core
	out vec2 UV;

	void main() {
		const vec4 positions[4] = vec4[](
          vec4(-1.0, 1.0, 1.0, 1.0),    // Top-left
          vec4(1.0, 1.0, 1.0, 1.0),     // Top-right
          vec4(-1.0, -1.0, 1.0, 1.0),   // Bottom-left
          vec4(1.0, -1.0, 1.0, 1.0)     // Bottom-right
        );

		// The 3DS displays both screens' framebuffer rotated 90 deg counter clockwise
		// So we adjust our texcoords accordingly
		const vec2 texcoords[4] = vec2[](
				vec2(1.0, 1.0), // Top-right
				vec2(1.0, 0.0), // Bottom-right
				vec2(0.0, 1.0), // Top-left
				vec2(0.0, 0.0)  // Bottom-left
	);

		gl_Position = positions[gl_VertexID];
	UV = texcoords[gl_VertexID];
	}
)";

const char* displayFragmentShader = R"(
    #version 410 core
    in vec2 UV;
    out vec4 FragColor;

    uniform sampler2D u_texture;
    void main() {
		FragColor = texture(u_texture, UV);
    }
)";

static void APIENTRY debugHandler(GLenum source, GLenum type, GLuint id, GLenum severity,
								  GLsizei length, const GLchar* message, const void* userParam) {
	Helpers::warn("%d: %s\n", id, message);
}


void Renderer::reset() {
	depthBufferCache.reset();
	colourBufferCache.reset();
	textureCache.reset();

	// Init the colour/depth buffer settings to some random defaults on reset
	colourBufferLoc = 0;
	colourBufferFormat = PICA::ColorFmt::RGBA8;

	depthBufferLoc = 0;
	depthBufferFormat = PICA::DepthFmt::Depth16;

	if (triangleProgram.exists()) {
		const auto oldProgram = OpenGL::getProgram();

		gl.useProgram(triangleProgram);
		
		oldDepthScale = -1.0; // Default depth scale to -1.0, which is what games typically use
		oldDepthOffset = 0.0; // Default depth offset to 0
		oldDepthmapEnable = false; // Enable w buffering

		glUniform1f(depthScaleLoc, oldDepthScale);
		glUniform1f(depthOffsetLoc, oldDepthOffset);
		glUniform1i(depthmapEnableLoc, oldDepthmapEnable);

		gl.useProgram(oldProgram);  // Switch to old GL program
	}
}

void Renderer::initGraphicsContext() {
	OpenGL::Shader vert(vertexShader, OpenGL::Vertex);
	OpenGL::Shader frag(fragmentShader, OpenGL::Fragment);
	triangleProgram.create({ vert, frag });
	gl.useProgram(triangleProgram);

	textureEnvSourceLoc = OpenGL::uniformLocation(triangleProgram, "u_textureEnvSource");
	textureEnvOperandLoc = OpenGL::uniformLocation(triangleProgram, "u_textureEnvOperand");
	textureEnvCombinerLoc = OpenGL::uniformLocation(triangleProgram, "u_textureEnvCombiner");
	textureEnvColorLoc = OpenGL::uniformLocation(triangleProgram, "u_textureEnvColor");
	textureEnvScaleLoc = OpenGL::uniformLocation(triangleProgram, "u_textureEnvScale");

	depthScaleLoc = OpenGL::uniformLocation(triangleProgram, "u_depthScale");
	depthOffsetLoc = OpenGL::uniformLocation(triangleProgram, "u_depthOffset");
	depthmapEnableLoc = OpenGL::uniformLocation(triangleProgram, "u_depthmapEnable");
	picaRegLoc = OpenGL::uniformLocation(triangleProgram, "u_picaRegs");

	// Init sampler objects. Texture 0 goes in texture unit 0, texture 1 in TU 1, texture 2 in TU 2, and the light maps go in TU 3
	glUniform1i(OpenGL::uniformLocation(triangleProgram, "u_tex0"), 0);
	glUniform1i(OpenGL::uniformLocation(triangleProgram, "u_tex1"), 1);
	glUniform1i(OpenGL::uniformLocation(triangleProgram, "u_tex2"), 2);
	glUniform1i(OpenGL::uniformLocation(triangleProgram, "u_tex_lighting_lut"), 3);

	OpenGL::Shader vertDisplay(displayVertexShader, OpenGL::Vertex);
	OpenGL::Shader fragDisplay(displayFragmentShader, OpenGL::Fragment);
	displayProgram.create({ vertDisplay, fragDisplay });

	gl.useProgram(displayProgram);
	glUniform1i(OpenGL::uniformLocation(displayProgram, "u_texture"), 0); // Init sampler object

	vbo.createFixedSize(sizeof(Vertex) * vertexBufferSize, GL_STREAM_DRAW);
	gl.bindVBO(vbo);
	vao.create();
	gl.bindVAO(vao);

	// Position (x, y, z, w) attributes
	vao.setAttributeFloat<float>(0, 4, sizeof(Vertex), offsetof(Vertex, s.positions));
	vao.enableAttribute(0);
	// Quaternion attribute
	vao.setAttributeFloat<float>(1, 4, sizeof(Vertex), offsetof(Vertex, s.quaternion));
	vao.enableAttribute(1);
	// Colour attribute
	vao.setAttributeFloat<float>(2, 4, sizeof(Vertex), offsetof(Vertex, s.colour));
	vao.enableAttribute(2);
	// UV 0 attribute
	vao.setAttributeFloat<float>(3, 2, sizeof(Vertex), offsetof(Vertex, s.texcoord0));
	vao.enableAttribute(3);
	// UV 1 attribute
	vao.setAttributeFloat<float>(4, 2, sizeof(Vertex), offsetof(Vertex, s.texcoord1));
	vao.enableAttribute(4);
	// UV 0 W-component attribute
	vao.setAttributeFloat<float>(5, 1, sizeof(Vertex), offsetof(Vertex, s.texcoord0_w));
	vao.enableAttribute(5);
	// View
	vao.setAttributeFloat<float>(6, 3, sizeof(Vertex), offsetof(Vertex, s.view));
	vao.enableAttribute(6);
	// UV 2 attribute
	vao.setAttributeFloat<float>(7, 2, sizeof(Vertex), offsetof(Vertex, s.texcoord2));
	vao.enableAttribute(7);

	dummyVBO.create();
	dummyVAO.create();

	// Create texture and framebuffer for the 3DS screen
	const u32 screenTextureWidth = 400; // Top screen is 400 pixels wide, bottom is 320
	const u32 screenTextureHeight = 2 * 240; // Both screens are 240 pixels tall
	
	glGenTextures(1,&lightLUTTextureArray);

	auto prevTexture = OpenGL::getTex2D();
	screenTexture.create(screenTextureWidth, screenTextureHeight, GL_RGBA8);
	screenTexture.bind();
	screenTexture.setMinFilter(OpenGL::Linear);
	screenTexture.setMagFilter(OpenGL::Linear);
	glBindTexture(GL_TEXTURE_2D, prevTexture);

	screenFramebuffer.createWithDrawTexture(screenTexture);
	screenFramebuffer.bind(OpenGL::DrawAndReadFramebuffer);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		Helpers::panic("Incomplete framebuffer");

	// TODO: This should not clear the framebuffer contents. It should load them from VRAM.
	GLint oldViewport[4];
	glGetIntegerv(GL_VIEWPORT, oldViewport);
	OpenGL::setViewport(screenTextureWidth, screenTextureHeight);
	OpenGL::setClearColor(0.0, 0.0, 0.0, 1.0);
	OpenGL::clearColor();
	OpenGL::setViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);

#if defined(OPENGL_DEBUG_INFO)
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(debugHandler, nullptr);
#endif

	reset();
}

// Set up the OpenGL blending context to match the emulated PICA
void Renderer::setupBlending() {
	const bool blendingEnabled = (regs[PICA::InternalRegs::ColourOperation] & (1 << 8)) != 0;
	
	// Map of PICA blending equations to OpenGL blending equations. The unused blending equations are equivalent to equation 0 (add)
	static constexpr std::array<GLenum, 8> blendingEquations = {
		GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT, GL_MIN, GL_MAX, GL_FUNC_ADD, GL_FUNC_ADD, GL_FUNC_ADD
	};
	
	// Map of PICA blending funcs to OpenGL blending funcs. Func = 15 is undocumented and stubbed to GL_ONE for now
	static constexpr std::array<GLenum, 16> blendingFuncs = {
		GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
		GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_CONSTANT_COLOR, GL_ONE_MINUS_CONSTANT_COLOR, GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA,
		GL_SRC_ALPHA_SATURATE, GL_ONE
	};

	if (!blendingEnabled) {
		gl.disableBlend();
	} else {
		gl.enableBlend();

		// Get blending equations
		const u32 blendControl = regs[PICA::InternalRegs::BlendFunc];
		const u32 rgbEquation = blendControl & 0x7;
		const u32 alphaEquation = getBits<8, 3>(blendControl);

		// Get blending functions
		const u32 rgbSourceFunc = getBits<16, 4>(blendControl);
		const u32 rgbDestFunc = getBits<20, 4>(blendControl);
		const u32 alphaSourceFunc = getBits<24, 4>(blendControl);
		const u32 alphaDestFunc = getBits<28, 4>(blendControl);

		const u32 constantColor = regs[PICA::InternalRegs::BlendColour];
		const u32 r = constantColor & 0xff;
		const u32 g = getBits<8, 8>(constantColor);
		const u32 b = getBits<16, 8>(constantColor);
		const u32 a = getBits<24, 8>(constantColor);
		OpenGL::setBlendColor(float(r) / 255.f, float(g) / 255.f, float(b) / 255.f, float(a) / 255.f);

		// Translate equations and funcs to their GL equivalents and set them
		glBlendEquationSeparate(blendingEquations[rgbEquation], blendingEquations[alphaEquation]);
		glBlendFuncSeparate(blendingFuncs[rgbSourceFunc], blendingFuncs[rgbDestFunc], blendingFuncs[alphaSourceFunc], blendingFuncs[alphaDestFunc]);
	}
}

void Renderer::setupTextureEnvState() {
	// TODO: Only update uniforms when the TEV config changed. Use an UBO potentially.

	static constexpr std::array<u32, 6> ioBases = {
	  PICA::InternalRegs::TexEnv0Source, PICA::InternalRegs::TexEnv1Source,
	  PICA::InternalRegs::TexEnv2Source, PICA::InternalRegs::TexEnv3Source,
	  PICA::InternalRegs::TexEnv4Source, PICA::InternalRegs::TexEnv5Source
	};

	u32 textureEnvSourceRegs[6];
	u32 textureEnvOperandRegs[6];
	u32 textureEnvCombinerRegs[6];
	u32 textureEnvColourRegs[6];
	u32 textureEnvScaleRegs[6];

	for (int i = 0; i < 6; i++) {
		const u32 ioBase = ioBases[i];

		textureEnvSourceRegs[i] = regs[ioBase];
		textureEnvOperandRegs[i] = regs[ioBase + 1];
		textureEnvCombinerRegs[i] = regs[ioBase + 2];
		textureEnvColourRegs[i] = regs[ioBase + 3];
		textureEnvScaleRegs[i] = regs[ioBase + 4];
	}

	glUniform1uiv(textureEnvSourceLoc, 6, textureEnvSourceRegs);
	glUniform1uiv(textureEnvOperandLoc, 6, textureEnvOperandRegs);
	glUniform1uiv(textureEnvCombinerLoc, 6, textureEnvCombinerRegs);
	glUniform1uiv(textureEnvColorLoc, 6, textureEnvColourRegs);
	glUniform1uiv(textureEnvScaleLoc, 6, textureEnvScaleRegs);
}

void Renderer::bindTexturesToSlots() {
	static constexpr std::array<u32, 3> ioBases = {
	  PICA::InternalRegs::Tex0BorderColor, PICA::InternalRegs::Tex1BorderColor, PICA::InternalRegs::Tex2BorderColor
	};

	for (int i = 0; i < 3; i++) {
		if ((regs[PICA::InternalRegs::TexUnitCfg] & (1 << i)) == 0) {
			continue;
		}

		const size_t ioBase = ioBases[i];

		const u32 dim = regs[ioBase + 1];
		const u32 config = regs[ioBase + 2];
		const u32 height = dim & 0x7ff;
		const u32 width = getBits<16, 11>(dim);
		const u32 addr = (regs[ioBase + 4] & 0x0FFFFFFF) << 3;
		u32 format = regs[ioBase + (i == 0 ? 13 : 5)] & 0xF;

		glActiveTexture(GL_TEXTURE0 + i);
		Texture targetTex(addr, static_cast<PICA::TextureFmt>(format), width, height, config);
		OpenGL::Texture tex = getTexture(targetTex);
		tex.bind();
	}

	glActiveTexture(GL_TEXTURE0 + 3);
	glBindTexture(GL_TEXTURE_1D_ARRAY, lightLUTTextureArray);
	glActiveTexture(GL_TEXTURE0);
}

void Renderer::updateLightingLUT() {
	gpu.lightingLUTDirty = false;
	std::array<u16, GPU::LightingLutSize> u16_lightinglut; 
	
	for (int i = 0; i < gpu.lightingLUT.size(); i++) {
		uint64_t value =  gpu.lightingLUT[i] & ((1 << 12) - 1);
		u16_lightinglut[i] = value * 65535 / 4095; 
	}

	glActiveTexture(GL_TEXTURE0 + 3);
	glBindTexture(GL_TEXTURE_1D_ARRAY, lightLUTTextureArray);
	glTexImage2D(GL_TEXTURE_1D_ARRAY, 0, GL_R16, 256, Lights::LUT_Count, 0, GL_RED, GL_UNSIGNED_SHORT, u16_lightinglut.data());
	glTexParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glActiveTexture(GL_TEXTURE0);
}

void Renderer::drawVertices(PICA::PrimType primType, std::span<const Vertex> vertices) {
	// The fourth type is meant to be "Geometry primitive". TODO: Find out what that is
	static constexpr std::array<OpenGL::Primitives, 4> primTypes = {
	  OpenGL::Triangle, OpenGL::TriangleStrip, OpenGL::TriangleFan, OpenGL::Triangle
	};
	const auto primitiveTopology = primTypes[static_cast<usize>(primType)];

	gl.disableScissor();
	gl.bindVBO(vbo);
	gl.bindVAO(vao);
	gl.useProgram(triangleProgram);

	OpenGL::enableClipPlane(0); // Clipping plane 0 is always enabled
	if (regs[PICA::InternalRegs::ClipEnable] & 1) {
		OpenGL::enableClipPlane(1);
	}

	setupBlending();
	OpenGL::Framebuffer poop = getColourFBO();
	poop.bind(OpenGL::DrawAndReadFramebuffer);

	const u32 depthControl = regs[PICA::InternalRegs::DepthAndColorMask];
	const bool depthEnable = depthControl & 1;
	const bool depthWriteEnable = getBit<12>(depthControl);
	const int depthFunc = getBits<4, 3>(depthControl);
	const int colourMask = getBits<8, 4>(depthControl);
	gl.setColourMask(colourMask & 1, colourMask & 2, colourMask & 4, colourMask & 8);

	static constexpr std::array<GLenum, 8> depthModes = {
		GL_NEVER, GL_ALWAYS, GL_EQUAL, GL_NOTEQUAL, GL_LESS, GL_LEQUAL, GL_GREATER, GL_GEQUAL
	};

	const float depthScale = f24::fromRaw(regs[PICA::InternalRegs::DepthScale] & 0xffffff).toFloat32();
	const float depthOffset = f24::fromRaw(regs[PICA::InternalRegs::DepthOffset] & 0xffffff).toFloat32();
	const bool depthMapEnable = regs[PICA::InternalRegs::DepthmapEnable] & 1;

	// Update depth uniforms
	if (oldDepthScale != depthScale) {
		oldDepthScale = depthScale;
		glUniform1f(depthScaleLoc, depthScale);
	}
	
	if (oldDepthOffset != depthOffset) {
		oldDepthOffset = depthOffset;
		glUniform1f(depthOffsetLoc, depthOffset);
	}

	if (oldDepthmapEnable != depthMapEnable) {
		oldDepthmapEnable = depthMapEnable;
		glUniform1i(depthmapEnableLoc, depthMapEnable);
	}

	setupTextureEnvState();
	bindTexturesToSlots();

	// Upload PICA Registers as a single uniform. The shader needs access to the rasterizer registers (for depth, starting from index 0x48)
	// The texturing and the fragment lighting registers. Therefore we upload them all in one go to avoid multiple slow uniform updates
	glUniform1uiv(picaRegLoc, 0x200 - 0x48, &regs[0x48]);

	if (gpu.lightingLUTDirty) {
		updateLightingLUT();
	}

	// TODO: Actually use this
	float viewportWidth = f24::fromRaw(regs[PICA::InternalRegs::ViewportWidth] & 0xffffff).toFloat32() * 2.0;
	float viewportHeight = f24::fromRaw(regs[PICA::InternalRegs::ViewportHeight] & 0xffffff).toFloat32() * 2.0;
	OpenGL::setViewport(viewportWidth, viewportHeight);

	// Note: The code below must execute after we've bound the colour buffer & its framebuffer
	// Because it attaches a depth texture to the aforementioned colour buffer
	if (depthEnable) {
		gl.enableDepth();
		gl.setDepthMask(depthWriteEnable ? GL_TRUE : GL_FALSE);
		gl.setDepthFunc(depthModes[depthFunc]);
		bindDepthBuffer();
	} else {
		if (depthWriteEnable) {
			gl.enableDepth();
			gl.setDepthMask(GL_TRUE);
			gl.setDepthFunc(GL_ALWAYS);
			bindDepthBuffer();
		} else {
			gl.disableDepth();
		}
	}

	vbo.bufferVertsSub(vertices);
	OpenGL::draw(primitiveTopology, vertices.size());
}

constexpr u32 topScreenBuffer = 0x1f000000;
constexpr u32 bottomScreenBuffer = 0x1f05dc00;

void Renderer::display() {
	gl.disableScissor();
	gl.disableBlend();
	gl.disableDepth();
	gl.disableScissor();
	gl.setColourMask(true, true, true, true);
	gl.useProgram(displayProgram);
	gl.bindVAO(dummyVAO);

	OpenGL::disableClipPlane(0);
	OpenGL::disableClipPlane(1);

	using namespace PICA::ExternalRegs;
	const u32 topScreenAddr = gpu.readExternalReg(Framebuffer0AFirstAddr);
	const u32 bottomScreenAddr = gpu.readExternalReg(Framebuffer1AFirstAddr);

	auto topScreen = colourBufferCache.findFromAddress(topScreenAddr);
	auto bottomScreen = colourBufferCache.findFromAddress(bottomScreenAddr);
	Helpers::warn("Top screen addr %08X\n", topScreenAddr);

	screenFramebuffer.bind(OpenGL::DrawFramebuffer);

	// Hack: Detect whether we are writing to the top or bottom screen by checking output gap and drawing to the proper part of the output texture
	// We consider output gap == 320 to mean bottom, and anything else to mean top
	if (topScreen) {
		topScreen->get().texture.bind();
		OpenGL::setViewport(0, 240, 400, 240); // Top screen viewport
		OpenGL::draw(OpenGL::TriangleStrip, 4); // Actually draw our 3DS screen
	}

	if (bottomScreen) {
		bottomScreen->get().texture.bind();
		OpenGL::setViewport(40, 0, 320, 240);
		OpenGL::draw(OpenGL::TriangleStrip, 4);
	}

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	screenFramebuffer.bind(OpenGL::ReadFramebuffer);
	glBlitFramebuffer(0, 0, 400, 480, 0, 0, 400, 480, GL_COLOR_BUFFER_BIT, GL_LINEAR);
}

void Renderer::clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) {
	return;
	log("GPU: Clear buffer\nStart: %08X End: %08X\nValue: %08X Control: %08X\n", startAddress, endAddress, value, control);

	const float r = float(getBits<24, 8>(value)) / 255.0;
	const float g = float(getBits<16, 8>(value)) / 255.0;
	const float b = float(getBits<8, 8>(value)) / 255.0;
	const float a = float(value & 0xff) / 255.0;

	if (startAddress == topScreenBuffer) {
		log("GPU: Cleared top screen\n");
	} else if (startAddress == bottomScreenBuffer) {
		log("GPU: Tried to clear bottom screen\n");
		return;
	} else {
		log("GPU: Clearing some unknown buffer\n");
	}

	OpenGL::setClearColor(r, g, b, a);
	OpenGL::clearColor();
}

OpenGL::Framebuffer Renderer::getColourFBO() {
	//We construct a colour buffer object and see if our cache has any matching colour buffers in it
	// If not, we allocate a texture & FBO for our framebuffer and store it in the cache 
	ColourBuffer sampleBuffer(colourBufferLoc, colourBufferFormat, fbSize.x(), fbSize.y());
	auto buffer = colourBufferCache.find(sampleBuffer);

	if (buffer.has_value()) {
		return buffer.value().get().fbo;
	} else {
		return colourBufferCache.add(sampleBuffer).fbo;
	}
}

void Renderer::bindDepthBuffer() {
	// Similar logic as the getColourFBO function
	DepthBuffer sampleBuffer(depthBufferLoc, depthBufferFormat, fbSize.x(), fbSize.y());
	auto buffer = depthBufferCache.find(sampleBuffer);
	GLuint tex;

	if (buffer.has_value()) {
		tex = buffer.value().get().texture.m_handle;
	} else {
		tex = depthBufferCache.add(sampleBuffer).texture.m_handle;
	}

	if (PICA::DepthFmt::Depth24Stencil8 != depthBufferFormat) {
		Helpers::panicDev("TODO: Should we remove stencil attachment?");
	}
	auto attachment = depthBufferFormat == PICA::DepthFmt::Depth24Stencil8 ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
	glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, tex, 0);
}

OpenGL::Texture Renderer::getTexture(Texture& tex) {
	// Similar logic as the getColourFBO/bindDepthBuffer functions
	auto buffer = textureCache.find(tex);

	if (buffer.has_value()) {
		return buffer.value().get().texture;
	} else {
		const std::span textureData = gpu.getSpanPhys<u8>(tex.location, tex.sizeInBytes()); // Get pointer to the texture data in 3DS memory
		Texture& newTex = textureCache.add(tex);
		newTex.decodeTexture(textureData);

		return newTex.texture;
	}
}

// NOTE: The GPU format has RGB5551 and RGB655 swapped compared to internal regs format
PICA::ColorFmt ToColorFmt(u32 format) {
	switch (format) {
		case 2: return PICA::ColorFmt::RGB565;
		case 3: return PICA::ColorFmt::RGBA5551;
		default: return static_cast<PICA::ColorFmt>(format);
	}
}

void Renderer::displayTransfer(u32 inputAddr, u32 outputAddr, u32 inputSize, u32 outputSize, u32 flags) {
	const u32 inputWidth = inputSize & 0xffff;
	const u32 inputHeight = inputSize >> 16;
	const auto inputFormat = ToColorFmt(Helpers::getBits<8, 3>(flags));
	const auto outputFormat = ToColorFmt(Helpers::getBits<12, 3>(flags));
	const PICA::Scaling scaling = static_cast<PICA::Scaling>(Helpers::getBits<24, 2>(flags));

	u32 outputWidth = outputSize & 0xffff;
	if (scaling == PICA::Scaling::X || scaling == PICA::Scaling::XY) {
		outputWidth >>= 1;
	}
	u32 outputHeight = outputSize >> 16;
	if (scaling == PICA::Scaling::XY) {
		outputHeight >>= 1;
	}

	// If there's a framebuffer at this address, use it. Otherwise go back to our old hack and display framebuffer 0
	// Displays are hard I really don't want to try implementing them because getting a fast solution is terrible
	auto srcFramebuffer = getColourBuffer(inputAddr, inputFormat, inputWidth, inputHeight);
	auto dstFramebuffer = getColourBuffer(outputAddr, outputFormat, outputWidth, outputHeight);

	Helpers::warn("Display transfer with outputAddr %08X\n", outputAddr);

	// Blit the framebuffers
	srcFramebuffer.fbo.bind(OpenGL::ReadFramebuffer);
	dstFramebuffer.fbo.bind(OpenGL::DrawFramebuffer);
	glBlitFramebuffer(0, 0, inputWidth, inputHeight, 0, 0, outputWidth, outputHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
}

void Renderer::textureCopy(u32 inputAddr, u32 outputAddr, u32 copyBytes, u32 inputSize, u32 outputSize, u32 flags) {
	copyBytes = Helpers::alignDown(copyBytes, 16);
	if (copyBytes == 0) [[unlikely]] {
		return;
	}

	const u32 inputWidth = (inputSize & 0xffff) * 16;
	const u32 inputGap = (inputSize >> 16) * 16;
	const u32 outputWidth = (outputSize & 0xffff) * 16;
	const u32 outputGap = (outputSize >> 16) * 16;

	if (inputGap != 0 || inputWidth != outputWidth) {
		Helpers::warn("Texture copy with non zero input gap or mismatching widths, cannot be accelerated");
		return;
	}

	// If the texture is tiled, apps set inputWidth to the scanline size which is width * 8.
	// HACK: We don't know if the src texture is tiled or not yet, assume it is for now, because it's the most common case.
	// Citra handles this by letting the width/stride be set as bytes and interpreting it differently
	// depending on the candidate surface.
	auto srcFramebuffer = getColourBuffer(inputAddr, PICA::ColorFmt::RGBA8, inputWidth / 8, copyBytes / inputWidth);  // HACK: Assume RGBA8 format
	auto dstFramebuffer = getColourBuffer(outputAddr, srcFramebuffer.format, outputWidth / 8, copyBytes / outputWidth);

	// Blit the framebuffers
	srcFramebuffer.fbo.bind(OpenGL::ReadFramebuffer);
	dstFramebuffer.fbo.bind(OpenGL::DrawFramebuffer);
	glBlitFramebuffer(0, 0, srcFramebuffer.size.x(), srcFramebuffer.size.y(),
					  0, 0, dstFramebuffer.size.x(), dstFramebuffer.size.y(), GL_COLOR_BUFFER_BIT, GL_LINEAR);
}

ColourBuffer Renderer::getColourBuffer(u32 addr, PICA::ColorFmt format, u32 width, u32 height) {
	// Try to find an already existing buffer that contains the provided address
	// This is a more relaxed check compared to getColourFBO as display transfer/texcopy may refer to
	// subrect of a surface and in case of texcopy we don't know the format of the surface.
	auto buffer = colourBufferCache.findFromAddress(addr);
	if (buffer.has_value()) {
		return buffer.value().get();
	}

	// Otherwise create and cache a new buffer.
	ColourBuffer sampleBuffer(addr, format, width, height);
	return colourBufferCache.add(sampleBuffer);
}

void Renderer::screenshot(const std::string& name) {
	constexpr uint width = 400;
	constexpr uint height = 2 * 240;

	std::vector<uint8_t> pixels, flippedPixels;
	pixels.resize(width *  height * 4);
	flippedPixels.resize(pixels.size());;

	OpenGL::bindScreenFramebuffer();
	glReadPixels(0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, pixels.data());

	// Flip the image vertically
	for (int y = 0; y < height; y++) {
		memcpy(&flippedPixels[y * width * 4], &pixels[(height - y - 1) * width * 4], width * 4);
		// Swap R and B channels
		for (int x = 0; x < width; x++) {
			std::swap(flippedPixels[y * width * 4 + x * 4 + 0], flippedPixels[y * width * 4 + x * 4 + 2]);
			// Set alpha to 0xFF
			flippedPixels[y * width * 4 + x * 4 + 3] = 0xFF;
		}
	}

	stbi_write_png(name.c_str(), width, height, 4, flippedPixels.data(), 0);
}
