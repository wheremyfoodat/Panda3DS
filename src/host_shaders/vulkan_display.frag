#version 460 core
layout(location = 0) in vec2 UV;
layout(location = 0) out vec4 FragColor;

layout (push_constant, std140) uniform DrawInfo {
    uint textureIndex;
};

layout (set = 0, binding = 0) uniform sampler2D u_textures[2];

void main() {
    FragColor = texture(u_textures[textureIndex], UV);
}