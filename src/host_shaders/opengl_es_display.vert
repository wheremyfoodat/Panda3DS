#version 310 es
precision mediump float;

out vec2 UV;

void main() {
	const vec4 positions[4] = vec4[](
		vec4(-1.0, 1.0, 1.0, 1.0),   // Top-left
		vec4(1.0, 1.0, 1.0, 1.0),    // Top-right
		vec4(-1.0, -1.0, 1.0, 1.0),  // Bottom-left
		vec4(1.0, -1.0, 1.0, 1.0)    // Bottom-right
	);

	// The 3DS displays both screens' framebuffer rotated 90 deg counter clockwise
	// So we adjust our texcoords accordingly
	const vec2 texcoords[4] = vec2[](
		vec2(1.0, 1.0),  // Top-right
		vec2(1.0, 0.0),  // Bottom-right
		vec2(0.0, 1.0),  // Top-left
		vec2(0.0, 0.0)   // Bottom-left
	);

	gl_Position = positions[gl_VertexID];
	UV = texcoords[gl_VertexID];
}