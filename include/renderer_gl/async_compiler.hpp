#pragma once

#include <atomic>
#include <thread>

#include "PICA/pica_frag_config.hpp"
#include "lockfree/spsc/queue.hpp"
#include "opengl.hpp"
#include "renderer_gl/renderer_gl.hpp"

namespace PICA::ShaderGen {
	class FragmentGenerator;
}

namespace AsyncCompiler {
	void* createContext(void* userdata);
	void makeCurrent(void* userdata, void* context);
	void destroyContext(void* context);
}  // namespace AsyncCompiler

struct CompilingProgram {
	CachedProgram* program;
	PICA::FragmentConfig* config;
};

struct AsyncCompilerThread {
	explicit AsyncCompilerThread(PICA::ShaderGen::FragmentGenerator& fragShaderGen, void* userdata);
	~AsyncCompilerThread();

	// Called from the emulator thread to queue a fragment configuration for compilation
	// Returns false if the queue is full, true otherwise
	void PushFragmentConfig(const PICA::FragmentConfig& config, CachedProgram* cachedProgram);

	// Wait for all queued fragment configurations to be compiled
	void Finish();

  private:
	PICA::ShaderGen::FragmentGenerator& fragShaderGen;
	OpenGL::Shader defaultShadergenVs;

	// Our lockfree queue only allows for trivial types, so we preallocate enough structs
	// to avoid dynamic allocation on each push
	int preallocatedProgramsIndex;
	static constexpr int preallocatedProgramsSize = 256;
	std::array<CompilingProgram*, preallocatedProgramsSize> preallocatedPrograms;
	lockfree::spsc::Queue<CompilingProgram*, preallocatedProgramsSize - 1> programQueue;
	std::atomic_bool running;
	std::atomic_bool hasWork;
	std::thread thread;
};