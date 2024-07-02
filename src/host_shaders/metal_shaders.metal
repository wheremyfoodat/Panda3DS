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

fragment float4 fragmentDraw(DrawVertexOut in [[stage_in]], texture2d<float> tex0 [[texture(0)]], texture2d<float> tex1 [[texture(1)]], texture2d<float> tex2 [[texture(2)]]) {
    // TODO: upload this as argument
    sampler samplr;

    float4 tevSources[16];
    tevSources[0] = in.color;
    // TODO: uncomment
	//calcLighting(tevSources[1], tevSources[2]);

	// TODO: uncomment
	//uint textureConfig = readPicaReg(0x80u);
	// HACK
	uint textureConfig = 0b111u;
	float2 texCoord2 = (textureConfig & (1u << 13)) != 0u ? in.texCoord1 : in.texCoord2;

	if ((textureConfig & 1u) != 0u) tevSources[3] = tex0.sample(samplr, in.texCoord0.xy);
	if ((textureConfig & 2u) != 0u) tevSources[4] = tex1.sample(samplr, in.texCoord1);
	if ((textureConfig & 4u) != 0u) tevSources[5] = tex2.sample(samplr, texCoord2);
	tevSources[13] = float4(0.0);  // Previous buffer
	tevSources[15] = in.color;     // Previous combiner

	// TODO: uncomment
	//float4 tevNextPreviousBuffer = v_textureEnvBufferColor;
	// HACK
	float4 tevNextPreviousBuffer = float4(0.0);
	// TODO: uncomment
	//uint textureEnvUpdateBuffer = readPicaReg(0xE0u);
	// HACK
	uint textureEnvUpdateBuffer = 0b111111u;

	for (int i = 0; i < 6; i++) {
	   // TODO: uncomment
		tevSources[14] = float4(0.0);//v_textureEnvColor[i];  // Constant color
		// TODO: uncomment
		tevSources[15] = float4(1.0);//tevCalculateCombiner(i);
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

	// HACK: should be tevSources[15]
	return tevSources[3];
}
