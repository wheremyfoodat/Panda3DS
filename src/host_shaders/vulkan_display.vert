#version 460 core
layout(location = 0) out vec2 UV;

void main() {
	UV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(UV * 2.0f + -1.0f, 0.0f, 1.0f);
}