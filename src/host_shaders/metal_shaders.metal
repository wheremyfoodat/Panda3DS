#include <metal_stdlib>
using namespace metal;

struct BasicVertexOut {
	float4 position [[position]];
	float2 uv;
};

constant float4 displayPositions[4] = {
    float4(-1.0, -1.0, 0.0, 1.0),
    float4( 1.0, -1.0, 0.0, 1.0),
    float4(-1.0,  1.0, 0.0, 1.0),
    float4( 1.0,  1.0, 0.0, 1.0)
};

constant float2 displayTexCoord[4] = {
    float2(0.0, 1.0),
    float2(0.0, 0.0),
    float2(1.0, 1.0),
    float2(1.0, 0.0)
};

vertex BasicVertexOut vertexDisplay(uint vid [[vertex_id]]) {
	BasicVertexOut out;
	out.position = displayPositions[vid];
	out.uv = displayTexCoord[vid];

	return out;
}

fragment float4 fragmentDisplay(BasicVertexOut in [[stage_in]], texture2d<float> tex [[texture(0)]], sampler samplr [[sampler(0)]]) {
	return tex.sample(samplr, in.uv);
}

vertex BasicVertexOut vertexBlit(uint vid [[vertex_id]]) {
	BasicVertexOut out;
	out.uv = float2((vid << 1) & 2, vid & 2);
	out.position = float4(out.uv * 2.0 - 1.0, 0.0, 1.0);
	out.position.y = -out.position.y;

	return out;
}

fragment float4 fragmentBlit(BasicVertexOut in [[stage_in]], texture2d<float> tex [[texture(0)]], sampler samplr [[sampler(0)]]) {
	return tex.sample(samplr, in.uv);
}

struct PicaRegs {
    uint regs[0x200 - 0x48];

    uint read(uint reg) constant {
        return regs[reg - 0x48];
    }
};

struct VertTEV {
    uint textureEnvColor[6];
};

float4 abgr8888ToFloat4(uint abgr) {
	const float scale = 1.0 / 255.0;

	return scale * float4(float(abgr & 0xffu), float((abgr >> 8) & 0xffu), float((abgr >> 16) & 0xffu), float(abgr >> 24));
}

struct DrawVertexIn {
	float4 position [[attribute(0)]];
	float4 quaternion [[attribute(1)]];
	float4 color [[attribute(2)]];
	float2 texCoord0 [[attribute(3)]];
	float2 texCoord1 [[attribute(4)]];
	float texCoord0W [[attribute(5)]];
	float3 view [[attribute(6)]];
	float2 texCoord2 [[attribute(7)]];
};

// Metal cannot return arrays from vertex functions, this is an ugly workaround
struct EnvColor {
    float4 c0;
    float4 c1;
    float4 c2;
    float4 c3;
    float4 c4;
    float4 c5;

    thread float4& operator[](int i) {
        switch (i) {
            case 0: return c0;
            case 1: return c1;
            case 2: return c2;
            case 3: return c3;
            case 4: return c4;
            case 5: return c5;
            default: return c0;
        }
    }
};

struct DrawVertexOut {
	float4 position [[position]];
	float4 color;
	float3 texCoord0;
	float2 texCoord1;
	float2 texCoord2;
	float3 view;
	float3 normal;
	float3 tangent;
	float3 bitangent;
	EnvColor textureEnvColor [[flat]];
	float4 textureEnvBufferColor [[flat]];
};

struct DrawVertexOutWithClip {
    DrawVertexOut out;
    float clipDistance [[clip_distance]] [2];
};

float3 rotateFloat3ByQuaternion(float3 v, float4 q) {
	float3 u = q.xyz;
	float s = q.w;

	return 2.0 * dot(u, v) * u + (s * s - dot(u, u)) * v + 2.0 * s * cross(u, v);
}

// Convert an arbitrary-width floating point literal to an f32
float decodeFP(uint hex, uint E, uint M) {
	uint width = M + E + 1u;
	uint bias = 128u - (1u << (E - 1u));
	uint exponent = (hex >> M) & ((1u << E) - 1u);
	uint mantissa = hex & ((1u << M) - 1u);
	uint sign = (hex >> (E + M)) << 31u;

	if ((hex & ((1u << (width - 1u)) - 1u)) != 0u) {
		if (exponent == (1u << E) - 1u)
			exponent = 255u;
		else
			exponent += bias;
		hex = sign | (mantissa << (23u - M)) | (exponent << 23u);
	} else {
		hex = sign;
	}

	return as_type<float>(hex);
}

struct DepthUniforms {
    float depthScale;
   	float depthOffset;
   	bool depthMapEnable;
};

// TODO: check this
float transformZ(float z, float w, constant DepthUniforms& depthUniforms) {
    z = z / w * depthUniforms.depthScale + depthUniforms.depthOffset;
    if (!depthUniforms.depthMapEnable) {
        z *= w;
    }

    return z * w;
}

vertex DrawVertexOutWithClip vertexDraw(DrawVertexIn in [[stage_in]], constant PicaRegs& picaRegs [[buffer(0)]], constant VertTEV& tev [[buffer(1)]], constant DepthUniforms& depthUniforms [[buffer(2)]]) {
	DrawVertexOut out;

	// Position
	out.position = in.position;
	// Flip the y position
	out.position.y = -out.position.y;

	// Apply depth uniforms
	out.position.z = transformZ(out.position.z, out.position.w, depthUniforms);

	// Color
	out.color = min(abs(in.color), 1.0);

	// Texture coordinates
	out.texCoord0 = float3(in.texCoord0, in.texCoord0W);
	out.texCoord0.y = 1.0 - out.texCoord0.y;
	out.texCoord1 = in.texCoord1;
	out.texCoord1.y = 1.0 - out.texCoord1.y;
	out.texCoord2 = in.texCoord2;
	out.texCoord2.y = 1.0 - out.texCoord2.y;

	// View
	out.view = in.view;

	// TBN
	out.normal = normalize(rotateFloat3ByQuaternion(float3(0.0, 0.0, 1.0), in.quaternion));
	out.tangent = normalize(rotateFloat3ByQuaternion(float3(1.0, 0.0, 0.0), in.quaternion));
	out.bitangent = normalize(rotateFloat3ByQuaternion(float3(0.0, 1.0, 0.0), in.quaternion));

	// Environment
	for (int i = 0; i < 6; i++) {
		out.textureEnvColor[i] = abgr8888ToFloat4(tev.textureEnvColor[i]);
	}

	out.textureEnvBufferColor = abgr8888ToFloat4(picaRegs.read(0xFDu));

	DrawVertexOutWithClip outWithClip;
	outWithClip.out = out;

	// Parse clipping plane registers
	float4 clipData = float4(
		decodeFP(picaRegs.read(0x48u) & 0xffffffu, 7u, 16u), decodeFP(picaRegs.read(0x49u) & 0xffffffu, 7u, 16u),
		decodeFP(picaRegs.read(0x4Au) & 0xffffffu, 7u, 16u), decodeFP(picaRegs.read(0x4Bu) & 0xffffffu, 7u, 16u)
	);

	// There's also another, always-on clipping plane based on vertex z
	// TODO: transform
	outWithClip.clipDistance[0] = -in.position.z;
	outWithClip.clipDistance[1] = dot(clipData, in.position);

	return outWithClip;
}

struct Globals {
    float4 tevSources[16];
    float4 tevNextPreviousBuffer;
    bool tevUnimplementedSourceFlag = false;
};

struct FragTEV {
    uint textureEnvSource[6];
    uint textureEnvOperand[6];
    uint textureEnvCombiner[6];
    uint textureEnvScale[6];

    float4 fetchSource(thread Globals& globals, uint src_id) constant {
    	if (src_id >= 6u && src_id < 13u) {
    		globals.tevUnimplementedSourceFlag = true;
    	}

    	return globals.tevSources[src_id];
    }

    float4 getColorAndAlphaSource(thread Globals& globals, int tev_id, int src_id) constant {
    	float4 result;

    	float4 colorSource = fetchSource(globals, (textureEnvSource[tev_id] >> (src_id * 4)) & 15u);
    	float4 alphaSource = fetchSource(globals, (textureEnvSource[tev_id] >> (src_id * 4 + 16)) & 15u);

    	uint colorOperand = (textureEnvOperand[tev_id] >> (src_id * 4)) & 15u;
    	uint alphaOperand = (textureEnvOperand[tev_id] >> (12 + src_id * 4)) & 7u;

    	// TODO: figure out what the undocumented values do
    	switch (colorOperand) {
    		case 0u: result.rgb = colorSource.rgb; break;             // Source color
    		case 1u: result.rgb = 1.0 - colorSource.rgb; break;       // One minus source color
    		case 2u: result.rgb = float3(colorSource.a); break;         // Source alpha
    		case 3u: result.rgb = float3(1.0 - colorSource.a); break;   // One minus source alpha
    		case 4u: result.rgb = float3(colorSource.r); break;         // Source red
    		case 5u: result.rgb = float3(1.0 - colorSource.r); break;   // One minus source red
    		case 8u: result.rgb = float3(colorSource.g); break;         // Source green
    		case 9u: result.rgb = float3(1.0 - colorSource.g); break;   // One minus source green
    		case 12u: result.rgb = float3(colorSource.b); break;        // Source blue
    		case 13u: result.rgb = float3(1.0 - colorSource.b); break;  // One minus source blue
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

    float4 calculateCombiner(thread Globals& globals, int tev_id) constant {
    	float4 source0 = getColorAndAlphaSource(globals, tev_id, 0);
    	float4 source1 = getColorAndAlphaSource(globals, tev_id, 1);
    	float4 source2 = getColorAndAlphaSource(globals, tev_id, 2);

    	uint colorCombine = textureEnvCombiner[tev_id] & 15u;
    	uint alphaCombine = (textureEnvCombiner[tev_id] >> 16) & 15u;

    	float4 result = float4(1.0);

    	// TODO: figure out what the undocumented values do
    	switch (colorCombine) {
    		case 0u: result.rgb = source0.rgb; break;                                            // Replace
    		case 1u: result.rgb = source0.rgb * source1.rgb; break;                              // Modulate
    		case 2u: result.rgb = min(float3(1.0), source0.rgb + source1.rgb); break;              // Add
    		case 3u: result.rgb = clamp(source0.rgb + source1.rgb - 0.5, 0.0, 1.0); break;       // Add signed
    		case 4u: result.rgb = mix(source1.rgb, source0.rgb, source2.rgb); break;             // Interpolate
    		case 5u: result.rgb = max(source0.rgb - source1.rgb, 0.0); break;                    // Subtract
    		case 6u: result.rgb = float3(4.0 * dot(source0.rgb - 0.5, source1.rgb - 0.5)); break;  // Dot3 RGB
    		case 7u: result = float4(4.0 * dot(source0.rgb - 0.5, source1.rgb - 0.5)); break;      // Dot3 RGBA
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

    	result.rgb *= float(1 << (textureEnvScale[tev_id] & 3u));
    	result.a *= float(1 << ((textureEnvScale[tev_id] >> 16) & 3u));

    	return result;
    }
};

enum class LogicOp : uint8_t {
    Clear = 0,
    And = 1,
    AndReverse = 2,
    Copy = 3,
    Set = 4,
    CopyInverted = 5,
    NoOp = 6,
    Invert = 7,
    Nand = 8,
    Or = 9,
    Nor = 10,
    Xor = 11,
    Equiv = 12,
    AndInverted = 13,
    OrReverse = 14,
    OrInverted = 15
};

uint4 performLogicOpU(LogicOp logicOp, uint4 s, uint4 d) {
    switch (logicOp) {
    case LogicOp::Clear: return as_type<uint4>(float4(0.0));
    case LogicOp::And: return s & d;
    case LogicOp::AndReverse: return s & ~d;
    case LogicOp::Copy: return s;
    case LogicOp::Set: return as_type<uint4>(float4(1.0));
    case LogicOp::CopyInverted: return ~s;
    case LogicOp::NoOp: return d;
    case LogicOp::Invert: return ~d;
    case LogicOp::Nand: return ~(s & d);
    case LogicOp::Or: return s | d;
    case LogicOp::Nor: return ~(s | d);
    case LogicOp::Xor: return s ^ d;
    case LogicOp::Equiv: return ~(s ^ d);
    case LogicOp::AndInverted: return ~s & d;
    case LogicOp::OrReverse: return s | ~d;
    case LogicOp::OrInverted: return ~s | d;
    }
}

#define D0_LUT 0u
#define D1_LUT 1u
#define SP_LUT 2u
#define FR_LUT 3u
#define RB_LUT 4u
#define RG_LUT 5u
#define RR_LUT 6u

float lutLookup(texture1d_array<float> texLightingLut, sampler linearSampler, uint lut, uint light, float value) {
	if (lut >= FR_LUT && lut <= RR_LUT) lut -= 1;
	if (lut == SP_LUT) lut = light + 8;

	return texLightingLut.sample(linearSampler, value, lut).r;
}

float3 regToColor(uint reg) {
	// Normalization scale to convert from [0...255] to [0.0...1.0]
	const float scale = 1.0 / 255.0;

	return scale * float3(float(extract_bits(reg, 20, 8)), float(extract_bits(reg, 10, 8)), float(extract_bits(reg, 00, 8)));
}

constant bool lightingEnabled [[function_constant(0)]];
constant uint8_t lightingNumLights [[function_constant(1)]];

// Implements the following algorthm: https://mathb.in/26766
void calcLighting(thread DrawVertexOut& in, constant PicaRegs& picaRegs, texture1d_array<float> texLightingLut, sampler linearSampler, thread float4& primaryColor, thread float4& secondaryColor) {
	// Quaternions describe a transformation from surface-local space to eye space.
	// In surface-local space, by definition (and up to permutation) the normal vector is (0,0,1),
	// the tangent vector is (1,0,0), and the bitangent vector is (0,1,0).
	float3 normal = normalize(in.normal);
	float3 tangent = normalize(in.tangent);
	float3 bitangent = normalize(in.bitangent);
	float3 view = normalize(in.view);

	uint GPUREG_LIGHTING_AMBIENT = picaRegs.read(0x01C0u);
	uint GPUREG_LIGHTING_LIGHT_PERMUTATION = picaRegs.read(0x01D9u);

	primaryColor = float4(float3(0.0), 1.0);
	secondaryColor = float4(float3(0.0), 1.0);

	primaryColor.rgb += regToColor(GPUREG_LIGHTING_AMBIENT);

	uint GPUREG_LIGHTING_LUTINPUT_ABS = picaRegs.read(0x01D0u);
	uint GPUREG_LIGHTING_LUTINPUT_SELECT = picaRegs.read(0x01D1u);
	uint GPUREG_LIGHTING_CONFIG0 = picaRegs.read(0x01C3u);
	uint GPUREG_LIGHTING_CONFIG1 = picaRegs.read(0x01C4u);
	uint GPUREG_LIGHTING_LUTINPUT_SCALE = picaRegs.read(0x01D2u);
	float d[7];

	bool errorUnimpl = false;

	for (uint i = 0u; i < lightingNumLights; i++) {
		uint lightID = extract_bits(GPUREG_LIGHTING_LIGHT_PERMUTATION, int(i * 3u), 3);

		uint GPUREG_LIGHTi_SPECULAR0 = picaRegs.read(0x0140u + 0x10u * lightID);
		uint GPUREG_LIGHTi_SPECULAR1 = picaRegs.read(0x0141u + 0x10u * lightID);
		uint GPUREG_LIGHTi_DIFFUSE = picaRegs.read(0x0142u + 0x10u * lightID);
		uint GPUREG_LIGHTi_AMBIENT = picaRegs.read(0x0143u + 0x10u * lightID);
		uint GPUREG_LIGHTi_VECTOR_LOW = picaRegs.read(0x0144u + 0x10u * lightID);
		uint GPUREG_LIGHTi_VECTOR_HIGH = picaRegs.read(0x0145u + 0x10u * lightID);
		uint GPUREG_LIGHTi_CONFIG = picaRegs.read(0x0149u + 0x10u * lightID);

		float3 lightVector = normalize(float3(
			decodeFP(extract_bits(GPUREG_LIGHTi_VECTOR_LOW, 0, 16), 5u, 10u), decodeFP(extract_bits(GPUREG_LIGHTi_VECTOR_LOW, 16, 16), 5u, 10u),
			decodeFP(extract_bits(GPUREG_LIGHTi_VECTOR_HIGH, 0, 16), 5u, 10u)
		));

		float3 halfVector;

		// Positional Light
		if (extract_bits(GPUREG_LIGHTi_CONFIG, 0, 1) == 0u) {
			// error_unimpl = true;
			halfVector = normalize(normalize(lightVector + in.view) + view);
		}

		// Directional light
		else {
			halfVector = normalize(normalize(lightVector) + view);
		}

		for (int c = 0; c < 7; c++) {
			if (extract_bits(GPUREG_LIGHTING_CONFIG1, 16 + c, 1) == 0u) {
				uint scaleID = extract_bits(GPUREG_LIGHTING_LUTINPUT_SCALE, c * 4, 3);
				float scale = float(1u << scaleID);
				if (scaleID >= 6u) scale /= 256.0;

				uint inputID = extract_bits(GPUREG_LIGHTING_LUTINPUT_SELECT, c * 4, 3);
				if (inputID == 0u)
					d[c] = dot(normal, halfVector);
				else if (inputID == 1u)
					d[c] = dot(view, halfVector);
				else if (inputID == 2u)
					d[c] = dot(normal, view);
				else if (inputID == 3u)
					d[c] = dot(lightVector, normal);
				else if (inputID == 4u) {
					uint GPUREG_LIGHTi_SPOTDIR_LOW = picaRegs.read(0x0146u + 0x10u * lightID);
					uint GPUREG_LIGHTi_SPOTDIR_HIGH = picaRegs.read(0x0147u + 0x10u * lightID);
					float3 spotLightVector = normalize(float3(
						decodeFP(extract_bits(GPUREG_LIGHTi_SPOTDIR_LOW, 0, 16), 1u, 11u),
						decodeFP(extract_bits(GPUREG_LIGHTi_SPOTDIR_LOW, 16, 16), 1u, 11u),
						decodeFP(extract_bits(GPUREG_LIGHTi_SPOTDIR_HIGH, 0, 16), 1u, 11u)
					));
					d[c] = dot(-lightVector, spotLightVector);  // -L dot P (aka Spotlight aka SP);
				} else if (inputID == 5u) {
					d[c] = 1.0;  // TODO: cos <greek symbol> (aka CP);
					errorUnimpl = true;
				} else {
					d[c] = 1.0;
				}

				d[c] = lutLookup(texLightingLut, linearSampler, uint(c), lightID, d[c] * 0.5 + 0.5) * scale;
				if (extract_bits(GPUREG_LIGHTING_LUTINPUT_ABS, 2 * c, 1) != 0u) d[c] = abs(d[c]);
			} else {
				d[c] = 1.0;
			}
		}

		uint lookupConfig = extract_bits(GPUREG_LIGHTi_CONFIG, 4, 4);
		if (lookupConfig == 0u) {
			d[D1_LUT] = 0.0;
			d[FR_LUT] = 0.0;
			d[RG_LUT] = d[RB_LUT] = d[RR_LUT];
		} else if (lookupConfig == 1u) {
			d[D0_LUT] = 0.0;
			d[D1_LUT] = 0.0;
			d[RG_LUT] = d[RB_LUT] = d[RR_LUT];
		} else if (lookupConfig == 2u) {
			d[FR_LUT] = 0.0;
			d[SP_LUT] = 0.0;
			d[RG_LUT] = d[RB_LUT] = d[RR_LUT];
		} else if (lookupConfig == 3u) {
			d[SP_LUT] = 0.0;
			d[RG_LUT] = d[RB_LUT] = d[RR_LUT] = 1.0;
		} else if (lookupConfig == 4u) {
			d[FR_LUT] = 0.0;
		} else if (lookupConfig == 5u) {
			d[D1_LUT] = 0.0;
		} else if (lookupConfig == 6u) {
			d[RG_LUT] = d[RB_LUT] = d[RR_LUT];
		}

		float distanceFactor = 1.0;  // a
		float indirectFactor = 1.0;  // fi
		float shadowFactor = 1.0;    // o

		float NdotL = dot(normal, lightVector);  // Li dot N

		// Two sided diffuse
		if (extract_bits(GPUREG_LIGHTi_CONFIG, 1, 1) == 0u)
			NdotL = max(0.0, NdotL);
		else
			NdotL = abs(NdotL);

		float light_factor = distanceFactor * d[SP_LUT] * indirectFactor * shadowFactor;

		primaryColor.rgb += light_factor * (regToColor(GPUREG_LIGHTi_AMBIENT) + regToColor(GPUREG_LIGHTi_DIFFUSE) * NdotL);
		secondaryColor.rgb += light_factor * (regToColor(GPUREG_LIGHTi_SPECULAR0) * d[D0_LUT] +
											   regToColor(GPUREG_LIGHTi_SPECULAR1) * d[D1_LUT] * float3(d[RR_LUT], d[RG_LUT], d[RB_LUT]));
	}
	uint fresnelOutput1 = extract_bits(GPUREG_LIGHTING_CONFIG0, 2, 1);
	uint fresnelOutput2 = extract_bits(GPUREG_LIGHTING_CONFIG0, 3, 1);

	if (fresnelOutput1 == 1u) primaryColor.a = d[FR_LUT];
	if (fresnelOutput2 == 1u) secondaryColor.a = d[FR_LUT];

	if (errorUnimpl) {
		// secondaryColor = primaryColor = float4(1.0, 0., 1.0, 1.0);
	}
}

float4 performLogicOp(LogicOp logicOp, float4 s, float4 d) {
    return as_type<float4>(performLogicOpU(logicOp, as_type<uint4>(s), as_type<uint4>(d)));
}

fragment float4 fragmentDraw(DrawVertexOut in [[stage_in]], float4 prevColor [[color(0)]], constant PicaRegs& picaRegs [[buffer(0)]], constant FragTEV& tev [[buffer(1)]], constant LogicOp& logicOp [[buffer(2)]],
                             texture2d<float> tex0 [[texture(0)]], texture2d<float> tex1 [[texture(1)]], texture2d<float> tex2 [[texture(2)]], texture1d_array<float> texLightingLut [[texture(3)]],
                             sampler samplr0 [[sampler(0)]], sampler samplr1 [[sampler(1)]], sampler samplr2 [[sampler(2)]], sampler linearSampler [[sampler(3)]]) {
    Globals globals;
    globals.tevSources[0] = in.color;
    if (lightingEnabled) {
        calcLighting(in, picaRegs, texLightingLut, linearSampler, globals.tevSources[1], globals.tevSources[2]);
    } else {
        globals.tevSources[1] = float4(0.0);
        globals.tevSources[2] = float4(0.0);
    }

	uint textureConfig = picaRegs.read(0x80u);
	float2 texCoord2 = (textureConfig & (1u << 13)) != 0u ? in.texCoord1 : in.texCoord2;

	if ((textureConfig & 1u) != 0u) globals.tevSources[3] = tex0.sample(samplr0, in.texCoord0.xy);
	if ((textureConfig & 2u) != 0u) globals.tevSources[4] = tex1.sample(samplr1, in.texCoord1);
	if ((textureConfig & 4u) != 0u) globals.tevSources[5] = tex2.sample(samplr2, texCoord2);
	globals.tevSources[13] = float4(0.0);  // Previous buffer
	globals.tevSources[15] = in.color;     // Previous combiner

	globals.tevNextPreviousBuffer = in.textureEnvBufferColor;
	uint textureEnvUpdateBuffer = picaRegs.read(0xE0u);

	for (int i = 0; i < 6; i++) {
		globals.tevSources[14] = in.textureEnvColor[i];  // Constant color
		globals.tevSources[15] = tev.calculateCombiner(globals, i);
		globals.tevSources[13] = globals.tevNextPreviousBuffer;

		if (i < 4) {
			if ((textureEnvUpdateBuffer & (0x100u << i)) != 0u) {
				globals.tevNextPreviousBuffer.rgb = globals.tevSources[15].rgb;
			}

			if ((textureEnvUpdateBuffer & (0x1000u << i)) != 0u) {
				globals.tevNextPreviousBuffer.a = globals.tevSources[15].a;
			}
		}
	}

	float4 color = performLogicOp(logicOp, globals.tevSources[15], prevColor);

	// Perform alpha test
	uint alphaControl = picaRegs.read(0x104u);
	if ((alphaControl & 1u) != 0u) {  // Check if alpha test is on
		uint func = (alphaControl >> 4u) & 7u;
		float reference = float((alphaControl >> 8u) & 0xffu) / 255.0;
		float alpha = color.a;

		switch (func) {
			case 0u: discard_fragment();  // Never pass alpha test
			case 1u: break;    // Always pass alpha test
			case 2u:           // Pass if equal
				if (alpha != reference) discard_fragment();
				break;
			case 3u:  // Pass if not equal
				if (alpha == reference) discard_fragment();
				break;
			case 4u:  // Pass if less than
				if (alpha >= reference) discard_fragment();
				break;
			case 5u:  // Pass if less than or equal
				if (alpha > reference) discard_fragment();
				break;
			case 6u:  // Pass if greater than
				if (alpha <= reference) discard_fragment();
				break;
			case 7u:  // Pass if greater than or equal
				if (alpha < reference) discard_fragment();
				break;
		}
	}

	return color;
}
