#version 300 es
precision mediump float;
precision mediump int;

layout(location = 0) in vec4 a_coords;
layout(location = 1) in vec4 a_quaternion;
layout(location = 2) in vec4 a_vertexColour;
layout(location = 3) in vec2 a_texcoord0;
layout(location = 4) in vec2 a_texcoord1;
layout(location = 5) in float a_texcoord0_w;
layout(location = 6) in vec3 a_view;
layout(location = 7) in vec2 a_texcoord2;

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

// out float gl_ClipDistance[2];

// TEV uniforms
uniform uint u_textureEnvColor[6];
uniform uint u_picaRegs[0x200 - 0x48];

// Helper so that the implementation of u_pica_regs can be changed later
uint readPicaReg(uint reg_addr) { return u_picaRegs[reg_addr - 0x48u]; }

vec4 abgr8888ToVec4(uint abgr) {
	const float scale = 1.0 / 255.0;

	return scale * vec4(float(abgr & 0xffu), float((abgr >> 8) & 0xffu), float((abgr >> 16) & 0xffu), float(abgr >> 24));
}

vec3 rotateVec3ByQuaternion(vec3 v, vec4 q) {
	vec3 u = q.xyz;
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

	return uintBitsToFloat(hex);
}

void main() {
	gl_Position = a_coords;
        vec4 colourAbs = abs(a_vertexColour);
        v_colour = min(colourAbs, vec4(1.f));

	// Flip y axis of UVs because OpenGL uses an inverted y for texture sampling compared to the PICA
	v_texcoord0 = vec3(a_texcoord0.x, 1.0 - a_texcoord0.y, a_texcoord0_w);
	v_texcoord1 = vec2(a_texcoord1.x, 1.0 - a_texcoord1.y);
	v_texcoord2 = vec2(a_texcoord2.x, 1.0 - a_texcoord2.y);
	v_view = a_view;

	v_normal = normalize(rotateVec3ByQuaternion(vec3(0.0, 0.0, 1.0), a_quaternion));
	v_tangent = normalize(rotateVec3ByQuaternion(vec3(1.0, 0.0, 0.0), a_quaternion));
	v_bitangent = normalize(rotateVec3ByQuaternion(vec3(0.0, 1.0, 0.0), a_quaternion));

	for (int i = 0; i < 6; i++) {
		v_textureEnvColor[i] = abgr8888ToVec4(u_textureEnvColor[i]);
	}

	v_textureEnvBufferColor = abgr8888ToVec4(readPicaReg(0xFDu));

	// Parse clipping plane registers
	// The plane registers describe a clipping plane in the form of Ax + By + Cz + D = 0
	// With n = (A, B, C) being the normal vector and D being the origin point distance
	// Therefore, for the second clipping plane, we can just pass the dot product of the clip vector and the input coordinates to gl_ClipDistance[1]
	vec4 clipData = vec4(
		decodeFP(readPicaReg(0x48u) & 0xffffffu, 7u, 16u), decodeFP(readPicaReg(0x49u) & 0xffffffu, 7u, 16u),
		decodeFP(readPicaReg(0x4Au) & 0xffffffu, 7u, 16u), decodeFP(readPicaReg(0x4Bu) & 0xffffffu, 7u, 16u)
	);

	// There's also another, always-on clipping plane based on vertex z
	// gl_ClipDistance[0] = -a_coords.z;
	// gl_ClipDistance[1] = dot(clipData, a_coords);
}
