#version 450 core

layout(location = 0) in vec4 a_coords;
layout(location = 1) in vec4 a_quaternion;
layout(location = 2) in vec4 a_vertexColour;
layout(location = 3) in vec2 a_texcoord0;
layout(location = 4) in vec2 a_texcoord1;
layout(location = 5) in float a_texcoord0_w;
layout(location = 6) in vec3 a_view;
layout(location = 7) in vec2 a_texcoord2;

layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec3 v_tangent;
layout(location = 2) out vec3 v_bitangent;
layout(location = 3) out vec4 v_colour;
layout(location = 4) out vec3 v_texcoord0;
layout(location = 5) out vec2 v_texcoord1;
layout(location = 6) out vec3 v_view;
layout(location = 7) out vec2 v_texcoord2;

out float gl_ClipDistance[2];

// The plane registers describe a clipping plane in the form of Ax + By + Cz + D = 0
// With n = (A, B, C) being the normal vector and D being the origin point distance
// Therefore, for the second clipping plane, we can just pass the dot product of the clip vector and the input coordinates to gl_ClipDistance[1]
layout(set = 0, binding = 0, std140) uniform vsSupportBuffer {
    vec4 clipData;
};

vec3 rotateVec3ByQuaternion(vec3 v, vec4 q) {
	vec3 u = q.xyz;
	float s = q.w;
	return 2.0 * dot(u, v) * u + (s * s - dot(u, u)) * v + 2.0 * s * cross(u, v);
}

void main() {
	gl_Position = vec4(a_coords.x, a_coords.y, -a_coords.z, a_coords.w);
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

	// There's also another, always-on clipping plane based on vertex z
	gl_ClipDistance[0] = -a_coords.z;
	gl_ClipDistance[1] = dot(clipData, a_coords);
}

