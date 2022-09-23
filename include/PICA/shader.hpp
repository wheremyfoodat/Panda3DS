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

namespace ShaderOpcodes {
	enum : u32 {
		MOV = 0x13
	};
}

class PICAShader {
	using f24 = Floats::f24;
	using vec4f = OpenGL::Vector<f24, 4>;

	int bufferIndex; // Index of the next instruction to overwrite for shader uploads
	int opDescriptorIndex; // Index of the next operand descriptor we'll overwrite
	std::array<u32, 128> operandDescriptors;
	std::array<vec4f, 16> tempRegisters;
	ShaderType type;

	// Shader opcodes
	void mov(u32 instruction);
	vec4f getSource(u32 source);
	vec4f& getDest(u32 dest);

	// src1, src2 and src3 have different negation & component swizzle bits in the operand descriptor
	// https://problemkaputt.github.io/gbatek.htm#3dsgpushaderinstructionsetopcodesummary in the
	// "Shader Operand Descriptors" section
	template <int sourceIndex>
	vec4f swizzle(vec4f& source, u32 opDescriptor) {
		vec4f ret;
		u32 compSwizzle;
		bool negate;

		if constexpr (sourceIndex == 1) { // SRC1
			negate = ((opDescriptor >> 4) & 1) != 0;
			compSwizzle = (opDescriptor >> 5) & 0xff;
		} else if constexpr (sourceIndex == 2) { // SRC2
			negate = ((opDescriptor >> 13) & 1) != 0;
			compSwizzle = (opDescriptor >> 14) & 0xff;
		} else if constexpr (sourceIndex == 3) { // SRC3
			negate = ((opDescriptor >> 22) & 1) != 0;
			compSwizzle = (opDescriptor >> 23) & 0xff;
		}

		// Iterate through every component of the swizzled vector in reverse order
		// And get which source component's index to match it with
		for (int comp = 0; comp < 4; comp++) {
			int index = compSwizzle & 3; // Get index for this component
			compSwizzle >>= 2; // Move to next component index
			ret[3 - comp] = source[index];
		}

		// Negate result if the negate bit is set
		if (negate) {
			ret[0] = -ret[0];
			ret[1] = -ret[1];
			ret[2] = -ret[2];
			ret[3] = -ret[3];
		}

		return ret;
	}

public:
	std::array<u32, 512> loadedShader; // Currently loaded & active shader
	std::array<u32, 512> bufferedShader; // Shader to be transferred when the SH_CODETRANSFER_END reg gets written to
	
	u32 boolUniform;
	std::array<u32, 4> intUniforms;
	std::array<vec4f, 8> floatUniforms;

	std::array<vec4f, 16> fixedAttributes; // Fixed vertex attributes
	std::array<vec4f, 16> attributes; // Attributes past to the shader
	std::array<vec4f, 16> outputs;

	PICAShader(ShaderType type) : type(type) {}

	void reset() {
		loadedShader.fill(0);
		bufferedShader.fill(0);
		operandDescriptors.fill(0);

		intUniforms.fill(0);
		boolUniform = 0;
		bufferIndex = 0;
		opDescriptorIndex = 0;

		const vec4f zero = vec4f({ f24::fromFloat32(0.0), f24::fromFloat32(0.0), f24::fromFloat32(0.0), f24::fromFloat32(0.0) });
		attributes.fill(zero);
		floatUniforms.fill(zero);
		outputs.fill(zero);
		tempRegisters.fill(zero);
	}

	void finalize() {
		std::memcpy(&loadedShader[0], &bufferedShader[0], 512 * sizeof(u32));
	}

	void setBufferIndex(u32 offset) {
		if (offset != 0) Helpers::panic("Is this register 9 or 11 bit?");
		bufferIndex = (offset >> 2) & 0x1ff;
	}

	void setOpDescriptorIndex(u32 offset) {
		opDescriptorIndex = offset & 0x7f;
	}

	void uploadWord(u32 word) {
		bufferedShader[bufferIndex++] = word;
		bufferIndex &= 0x1ff;
	}

	void uploadDescriptor(u32 word) {
		operandDescriptors[opDescriptorIndex++] = word;
		opDescriptorIndex &= 0x7f;
	}

	void run();
};