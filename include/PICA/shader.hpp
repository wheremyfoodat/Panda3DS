#pragma once
#include <algorithm>
#include <array>
#include <cstring>
#include "helpers.hpp"

class PICAShader {
	int bufferIndex; // Index of the next instruction to overwrite

public:
	std::array<u32, 512> loadedShader; // Currently loaded & active shader
	std::array<u32, 512> bufferedShader; // Shader to be transferred when the SH_CODETRANSFER_END reg gets written to
	
	u32 boolUniform;
	std::array<u32, 4> intUniforms;
	std::array<u32, 8> floatUniforms;

	void reset() {
		loadedShader.fill(0);
		bufferedShader.fill(0);

		intUniforms.fill(0);
		floatUniforms.fill(0);
		boolUniform = 0;
		bufferIndex = 0;
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