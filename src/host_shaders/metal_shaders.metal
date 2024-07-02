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
};

vertex DrawVertexOut vertexDraw(DrawVertexIn in [[stage_in]]) {
	DrawVertexOut out;
	out.position = in.position;
	// in.position.z is in range of [-1 ... 1], convert it to [0 ... 1]
	out.position.z = (in.position.z + 1.0) * 0.5;
	out.color = in.color;

	return out;
}

fragment float4 fragmentDraw(DrawVertexOut in [[stage_in]]) { return in.color; }
