#include <metal_stdlib>
using namespace metal;

struct DisplayVertexOut {
	float4 position [[position]];
	float2 uv;
};

vertex DisplayVertexOut vertexDisplay(uint vid [[vertex_id]]) {
	DisplayVertexOut out;
	out.uv = float2((vid << 1) & 2, vid & 2);
	out.position = float4(out.uv * 2.0f + -1.0f, 0.0f, 1.0f);

	return out;
}

fragment float4 fragmentDisplay(DisplayVertexOut in [[stage_in]], texture2d<float> tex [[texture(0)]], sampler samplr [[sampler(0)]]) {
	return tex.sample(samplr, in.uv);
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

struct DrawVertexOut {
	float4 position [[position]];
	float4 color;
	float3 texCoord0;
	float2 texCoord1;
	float2 texCoord2;
};

vertex DrawVertexOut vertexDraw(DrawVertexIn in [[stage_in]]) {
	DrawVertexOut out;

	// Position
	out.position = in.position;
	// HACK: rotate the position
	out.position.xy = -out.position.yx;
	// in.position.z is in range of [-1 ... 1], convert it to [0 ... 1]
	out.position.z = (in.position.z + 1.0) * 0.5;

	// Color
	out.color = in.color;

	// Texture coordinates
	out.texCoord0 = float3(in.texCoord0, in.texCoord0W);
	out.texCoord0.y = 1.0 - out.texCoord0.y;
	out.texCoord1 = in.texCoord1;
	out.texCoord1.y = 1.0 - out.texCoord1.y;
	out.texCoord2 = in.texCoord2;
	out.texCoord2.y = 1.0 - out.texCoord2.y;

	return out;
}

struct Globals {
    float4 tevSources[16];
    float4 tevNextPreviousBuffer;
    bool tevUnimplementedSourceFlag = false;
};

struct PicaRegs {
    uint regs[0x200 - 0x48];

    uint read(uint reg) constant {
        return regs[reg - 0x48];
    }
};

struct TEV {
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

fragment float4 fragmentDraw(DrawVertexOut in [[stage_in]], constant PicaRegs& picaRegs [[buffer(0)]], constant TEV& tev [[buffer(1)]], texture2d<float> tex0 [[texture(0)]], texture2d<float> tex1 [[texture(1)]], texture2d<float> tex2 [[texture(2)]]) {
    // TODO: upload this as argument
    sampler samplr;

    Globals globals;
    globals.tevSources[0] = in.color;
    // TODO: uncomment
	//calcLighting(tevSources[1], tevSources[2]);

	uint textureConfig = picaRegs.read(0x80u);
	float2 texCoord2 = (textureConfig & (1u << 13)) != 0u ? in.texCoord1 : in.texCoord2;

	if ((textureConfig & 1u) != 0u) globals.tevSources[3] = tex0.sample(samplr, in.texCoord0.xy);
	if ((textureConfig & 2u) != 0u) globals.tevSources[4] = tex1.sample(samplr, in.texCoord1);
	if ((textureConfig & 4u) != 0u) globals.tevSources[5] = tex2.sample(samplr, texCoord2);
	globals.tevSources[13] = float4(0.0);  // Previous buffer
	globals.tevSources[15] = in.color;     // Previous combiner

	// TODO: uncomment
	//float4 tevNextPreviousBuffer = v_textureEnvBufferColor;
	// HACK
	globals.tevNextPreviousBuffer = float4(1.0);
	uint textureEnvUpdateBuffer = picaRegs.read(0xE0u);

	for (int i = 0; i < 6; i++) {
	    // TODO: uncomment
		globals.tevSources[14] = float4(1.0);//v_textureEnvColor[i];  // Constant color
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

	return globals.tevSources[15];
}
