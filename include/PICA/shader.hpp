#pragma once
#include <algorithm>
#include <array>
#include <cstring>
#include "helpers.hpp"
#include "opengl.hpp"
#include "PICA/float_types.hpp"

enum class ShaderType {
	Vertex, Geometry
};

template <ShaderType type>
class PICAShader {
	int bufferIndex; // Index of the next instruction to overwrite
	using f24 = Floats::f24;
	using vec4f = OpenGL::Vector<f24, 4>;

public:
	std::array<u32, 512> loadedShader; // Currently loaded & active shader
	std::array<u32, 512> bufferedShader; // Shader to be transferred when the SH_CODETRANSFER_END reg gets written to
	
	u32 boolUniform;
	std::array<u32, 4> intUniforms;
	std::array<vec4f, 8> floatUniforms;

	std::array<vec4f, 16> fixedAttributes; // Fixed vertex attributes
	std::array<vec4f, 16> attributes; // Attributes past to the shader
	std::array<vec4f, 16> outputs;

	void reset() {
		loadedShader.fill(0);
		bufferedShader.fill(0);

		intUniforms.fill(0);
		boolUniform = 0;
		bufferIndex = 0;

		const vec4f zero = vec4f({ f24::fromFloat32(0.0), f24::fromFloat32(0.0), f24::fromFloat32(0.0), f24::fromFloat32(0.0) });
		attributes.fill(zero);
		floatUniforms.fill(zero);
		outputs.fill(zero);
	}

	void finalize() {
		std::memcpy(&loadedShader[0], &bufferedShader[0], 512 * sizeof(u32));
	}

	void setBufferIndex(u32 offset) {
		if (offset != 0) Helpers::panic("Is this register 9 or 11 bit?");
		bufferIndex = (offset >> 2) & 511;
	}

	void uploadWord(u32 word) {
		bufferedShader[bufferIndex++] = word;
		bufferIndex &= 511;
	}
};