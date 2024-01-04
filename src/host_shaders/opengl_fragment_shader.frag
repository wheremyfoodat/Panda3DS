#version 300 es
precision mediump float;

in vec3 v_tangent;
in vec3 v_normal;
in vec3 v_bitangent;
in vec4 v_colour;
in vec3 v_texcoord0;
in vec2 v_texcoord1;
in vec3 v_view;
in vec2 v_texcoord2;
flat in vec4 v_textureEnvColor[6];
flat in vec4 v_textureEnvBufferColor;

out vec4 fragColour;

// TEV uniforms
uniform uint u_textureEnvSource[6];
uniform uint u_textureEnvOperand[6];
uniform uint u_textureEnvCombiner[6];
uniform uint u_textureEnvScale[6];

// Depth control uniforms
uniform float u_depthScale;
uniform float u_depthOffset;
uniform bool u_depthmapEnable;

uniform sampler2D u_tex0;
uniform sampler2D u_tex1;
uniform sampler2D u_tex2;
// uniform sampler1DArray u_tex_lighting_lut;

uniform uint u_picaRegs[0x200 - 0x48];

// Helper so that the implementation of u_pica_regs can be changed later
uint readPicaReg(uint reg_addr) { return u_picaRegs[reg_addr - 0x48u]; }

vec4 tevSources[16];
vec4 tevNextPreviousBuffer;
bool tevUnimplementedSourceFlag = false;

// OpenGL ES 1.1 reference pages for TEVs (this is what the PICA200 implements):
// https://registry.khronos.org/OpenGL-Refpages/es1.1/xhtml/glTexEnv.xml

void main() {
	uint textureConfig = readPicaReg(0x80u);

	if ((textureConfig & 1u) != 0u) {
		fragColour = texture(u_tex0, v_texcoord0.xy);
	} else {
		fragColour = vec4(0.0f);
	}
}
