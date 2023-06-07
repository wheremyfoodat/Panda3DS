#pragma once
#include "helpers.hpp"
#include "renderer_gl/renderer_gl.hpp"
#include "x64_regs.hpp"

// Recompiler that takes the current vertex attribute configuration, ie the format of vertices (VAO in OpenGL) and emits optimized
// code in our CPU's native architecture for loading vertices

class VertexLoaderJIT {
	using PICARegs = const std::array<u32, 0x300>&;
	using Callback = void(*)(Vertex* output, size_t count);  // A function pointer to JIT-emitted code
	Callback compileConfig(PICARegs regs);

public:
#if defined(PANDA3DS_DYNAPICA_SUPPORTED) && defined(PANDA3DS_X64_HOST)
	#define PANDA3DS_VERTEX_LOADER_JIT_SUPPORTED

	void loadVertices(Vertex* output, size_t count, PICARegs regs);
	static constexpr bool isAvailable() { return true; }

#else
	void loadVertices(Vertex* output, size_t count, PICARegs regs) {
		Helpers::panic("Vertex Loader JIT: Tried to load vertices with JIT on platform that does not support vertex loader jit");
	}

	static constexpr bool isAvailable() { return false; }
#endif
};