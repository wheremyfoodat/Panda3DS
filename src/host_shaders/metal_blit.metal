#include <metal_stdlib>
using namespace metal;

#define GET_HELPER_TEXTURE_BINDING(binding) (30 - binding)
#define GET_HELPER_SAMPLER_STATE_BINDING(binding) (15 - binding)

struct BasicVertexOut {
	float4 position [[position]];
	float2 uv;
};

struct NDCViewport {
	float2 offset;
	float2 scale;
};

vertex BasicVertexOut vertexBlit(uint vid [[vertex_id]], constant NDCViewport& viewport [[buffer(0)]]) {
	BasicVertexOut out;
	out.uv = float2((vid << 1) & 2, vid & 2);
	out.position = float4(out.uv * 2.0 - 1.0, 0.0, 1.0);
	out.position.y = -out.position.y;
	out.uv = out.uv * viewport.scale + viewport.offset;

	return out;
}

fragment float4 fragmentBlit(BasicVertexOut in [[stage_in]], texture2d<float> tex [[texture(GET_HELPER_TEXTURE_BINDING(0))]], sampler samplr [[sampler(GET_HELPER_SAMPLER_STATE_BINDING(0))]]) {
	return tex.sample(samplr, in.uv);
}
