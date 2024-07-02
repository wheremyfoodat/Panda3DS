struct VertexOut {
    float4 position [[position]];
    float2 uv;
};

vertex VertexOut vertexMain(uint vid [[vertex_id]]) {
    VertexOut out;
	out.uv = float2((vid << 1) & 2, vid & 2);
	out.position = float4(out.uv * 2.0f + -1.0f, 0.0f, 1.0f);

	return out;
}

fragment float4 fragmentMain(VertexOut in [[stage_in]], texture2d<float> tex [[texture(0)]], sampler samplr [[sampler(0)]]) {
    return tex.sample(samplr, in.uv);
}
