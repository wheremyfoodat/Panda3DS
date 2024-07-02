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
	float4 color [[attribute(2)]];
};

struct DrawVertexOut {
	float4 position [[position]];
	float4 color;
};

vertex DrawVertexOut vertexDraw(DrawVertexIn in [[stage_in]]) {
	DrawVertexOut out;
	out.position = float4(in.position.xy, 0.0, 1.0);  // HACK
	out.color = in.color;

	return out;
}

fragment float4 fragmentDraw(DrawVertexOut in [[stage_in]]) { return in.color; }
