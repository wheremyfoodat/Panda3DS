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
		ADD = 0x00,
		DP3 = 0x01,
		DP4 = 0x02,
		MUL = 0x08,
		SLT = 0x0A,
		FLR = 0x0B,
		MAX = 0x0C,
		MIN = 0x0D,
		RCP = 0x0E,
		RSQ = 0x0F,
		MOVA = 0x12,
		MOV = 0x13,
		SGEI = 0x1A,
		SLTI = 0x1B,
		NOP = 0x21,
		END = 0x22,
		CALL = 0x24,
		CALLC = 0x25,
		CALLU = 0x26,
		IFU = 0x27,
		IFC = 0x28,
		LOOP = 0x29,
		JMPC = 0x2C,
		JMPU = 0x2D,
		CMP1 = 0x2E, // Both of these instructions are CMP
		CMP2 = 0x2F,
		MAD = 0x38 // Everything between 0x38-0x3F is a MAD but fuck it
	};
}

// Note: All PICA f24 vec4 registers must have the alignas(16) specifier to make them easier to access in SSE/NEON code in the JIT
class PICAShader {
	using f24 = Floats::f24;
	using vec4f = OpenGL::Vector<f24, 4>;

	struct Loop {
		u32 startingPC; // PC at the start of the loop
		u32 endingPC;   // PC at the end of the loop
		u32 iterations; // How many iterations of the loop to run
		u32 increment;  // How much to increment the loop counter after each iteration
	};

	// Info for ifc/ifu stack
	struct ConditionalInfo {
		u32 endingPC; // PC at the end of the if block (= DST)
		u32 newPC; // PC after the if block is done executing (= DST + NUM)
	};

	struct CallInfo {
		u32 endingPC; // PC at the end of the function
		u32 returnPC; // PC to return to after the function ends
	};

	int bufferIndex; // Index of the next instruction to overwrite for shader uploads
	int opDescriptorIndex; // Index of the next operand descriptor we'll overwrite
	u32 floatUniformIndex = 0; // Which float uniform are we writing to? ([0, 95] range)
	u32 floatUniformWordCount = 0; // How many words have we buffered for the current uniform transfer?
	bool f32UniformTransfer = false; // Are we transferring an f32 uniform or an f24 uniform?

	std::array<u32, 4> floatUniformBuffer; // Buffer for temporarily caching float uniform data

protected:
	std::array<u32, 128> operandDescriptors;
	alignas(16) std::array<vec4f, 16> tempRegisters; // General purpose registers the shader can use for temp values
	OpenGL::Vector<s32, 2> addrRegister; // Address register
	bool cmpRegister[2]; // Comparison registers where the result of CMP is stored in
	u32 loopCounter;

	u32 pc = 0; // Program counter: Index of the next instruction we're going to execute
	u32 loopIndex = 0; // The index of our loop stack (0 = empty, 4 = full)
	u32 ifIndex = 0; // The index of our IF stack
	u32 callIndex = 0; // The index of our CALL stack

	std::array<Loop, 4> loopInfo;
	std::array<ConditionalInfo, 8> conditionalInfo;
	std::array<CallInfo, 4> callInfo;
	ShaderType type;

	// We use a hashmap for matching 3DS shaders to their equivalent compiled code in our shader cache in the shader JIT
	// We choose our hash type to be a 64-bit integer by default, as the collision chance is very tiny and generating it is decently optimal
	// Ideally we want to be able to support multiple different types of hash depending on compilation settings, but let's get this working first
	using Hash = u64;

	Hash lastCodeHash = 0; // Last hash computed for the shader code (Used for the JIT caching mechanism)
	Hash lastOpdescHash = 0;  // Last hash computed for the operand descriptors (Also used for the JIT)

	bool codeHashDirty = false;
	bool opdescHashDirty = false;

	// Add these as friend classes for the JIT so it has access to all important state
	friend class ShaderJIT;
	friend class ShaderEmitter;

	vec4f getSource(u32 source);
	vec4f& getDest(u32 dest);

private:
	// Interpreter functions for the various shader functions
	void add(u32 instruction);
	void call(u32 instruction);
	void callc(u32 instruction);
	void callu(u32 instruction);
	void cmp(u32 instruction);
	void dp3(u32 instruction);
	void dp4(u32 instruction);
	void flr(u32 instruction);
	void ifc(u32 instruction);
	void ifu(u32 instruction);
	void jmpc(u32 instruction);
	void jmpu(u32 instruction);
	void loop(u32 instruction);
	void mad(u32 instruction);
	void madi(u32 instruction);
	void max(u32 instruction);
	void min(u32 instruction);
	void mov(u32 instruction);
	void mova(u32 instruction);
	void mul(u32 instruction);
	void rcp(u32 instruction);
	void rsq(u32 instruction);
	void sgei(u32 instruction);
	void slt(u32 instruction);
	void slti(u32 instruction);

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

	template <int sourceIndex>
	vec4f getSourceSwizzled(u32 source, u32 opDescriptor) {
		vec4f srcVector = getSource(source);
		srcVector = swizzle<sourceIndex>(srcVector, opDescriptor);

		return srcVector;
	}

	u8 getIndexedSource(u32 source, u32 index);
	bool isCondTrue(u32 instruction);

public:
	static constexpr size_t maxInstructionCount = 4096;
	std::array<u32, maxInstructionCount> loadedShader; // Currently loaded & active shader
	std::array<u32, maxInstructionCount> bufferedShader; // Shader to be transferred when the SH_CODETRANSFER_END reg gets written to

	u32 entrypoint = 0; // Initial shader PC
	u32 boolUniform;
	std::array<OpenGL::Vector<u8, 4>, 4> intUniforms;
	alignas(16) std::array<vec4f, 96> floatUniforms;

	alignas(16) std::array<vec4f, 16> fixedAttributes; // Fixed vertex attributes
	alignas(16) std::array<vec4f, 16> inputs; // Attributes passed to the shader
	alignas(16) std::array<vec4f, 16> outputs;

	PICAShader(ShaderType type) : type(type) {}

	// Theese functions are in the header to be inlined more easily, though with LTO I hope I'll be able to move them
	void finalize() {
		std::memcpy(&loadedShader[0], &bufferedShader[0], 4096 * sizeof(u32));
	}

	void setBufferIndex(u32 index) {
		bufferIndex = index & 0xfff;
	}

	void setOpDescriptorIndex(u32 index) {
		opDescriptorIndex = index & 0x7f;
	}

	void uploadWord(u32 word) {
		if (bufferIndex >= 4095) Helpers::panic("o no, shader upload overflew");
		bufferedShader[bufferIndex++] = word;
		bufferIndex &= 0xfff;

		codeHashDirty = true; // Signal the JIT if necessary that the program hash has potentially changed
	}

	void uploadDescriptor(u32 word) {
		operandDescriptors[opDescriptorIndex++] = word;
		opDescriptorIndex &= 0x7f;

		opdescHashDirty = true; // Signal the JIT if necessary that the program hash has potentially changed
	}

	void setFloatUniformIndex(u32 word) {
		floatUniformIndex = word & 0xff;
		floatUniformWordCount = 0;
		f32UniformTransfer = (word & 0x80000000) != 0;
	}

	void uploadFloatUniform(u32 word) {
		floatUniformBuffer[floatUniformWordCount++] = word;
		if (floatUniformIndex >= 96)
			Helpers::panic("[PICA] Tried to write float uniform %d", floatUniformIndex);

		if ((f32UniformTransfer && floatUniformWordCount >= 4) || (!f32UniformTransfer && floatUniformWordCount >= 3)) {
			vec4f& uniform = floatUniforms[floatUniformIndex++];
			floatUniformWordCount = 0;

			if (f32UniformTransfer) {
				uniform.x() = f24::fromFloat32(*(float*)&floatUniformBuffer[3]);
				uniform.y() = f24::fromFloat32(*(float*)&floatUniformBuffer[2]);
				uniform.z() = f24::fromFloat32(*(float*)&floatUniformBuffer[1]);
				uniform.w() = f24::fromFloat32(*(float*)&floatUniformBuffer[0]);
			} else {
				uniform.x() = f24::fromRaw(floatUniformBuffer[2] & 0xffffff);
				uniform.y() = f24::fromRaw(((floatUniformBuffer[1] & 0xffff) << 8) | (floatUniformBuffer[2] >> 24));
				uniform.z() = f24::fromRaw(((floatUniformBuffer[0] & 0xff) << 16) | (floatUniformBuffer[1] >> 16));
				uniform.w() = f24::fromRaw(floatUniformBuffer[0] >> 8);
			}
		}
	}

	void uploadIntUniform(int index, u32 word) {
		auto& u = intUniforms[index];
		u.x() = word & 0xff;
		u.y() = (word >> 8) & 0xff;
		u.z() = (word >> 16) & 0xff;
		u.w() = (word >> 24) & 0xff;
	}

	void run();
	void reset();

	Hash getCodeHash();
	Hash getOpdescHash();
};