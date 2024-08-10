#include "renderer_gl/async_compiler.hpp"

AsyncCompilerThread::AsyncCompilerThread(PICA::ShaderGen::FragmentGenerator& fragShaderGen, void* userdata) : fragShaderGen(fragShaderGen) {
	preallocatedProgramsIndex = 0;
	running.store(true);

	for (int i = 0; i < preallocatedProgramsSize; i++) {
		preallocatedPrograms[i] = new CompilingProgram();
		preallocatedPrograms[i]->config = new PICA::FragmentConfig({});
	}

	// The context needs to be created on the main thread so that we can make it shared with that
	// thread's context
	void* context = AsyncCompiler::createContext(userdata);
	thread = std::thread([this, userdata, context]() {
		AsyncCompiler::makeCurrent(userdata, context);
		printf("Async compiler started, GL version: %s\n", glGetString(GL_VERSION));

		std::string defaultShadergenVSSource = this->fragShaderGen.getDefaultVertexShader();
		defaultShadergenVs.create({defaultShadergenVSSource.c_str(), defaultShadergenVSSource.size()}, OpenGL::Vertex);

		while (running.load()) {
			CompilingProgram* item;
			while (programQueue.Pop(item)) {
				OpenGL::Program& glProgram = item->program->program;
				std::string fs = this->fragShaderGen.generate(*item->config);
				OpenGL::Shader fragShader({fs.c_str(), fs.size()}, OpenGL::Fragment);
				glProgram.create({defaultShadergenVs, fragShader});
				item->program->compiling.store(false);
				fragShader.free();
			}

			hasWork.store(false);
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		AsyncCompiler::destroyContext(context);
	});
}

AsyncCompilerThread::~AsyncCompilerThread() {
	running.store(false);
	thread.join();

	for (int i = 0; i < preallocatedProgramsSize; i++) {
		delete preallocatedPrograms[i]->config;
		delete preallocatedPrograms[i];
	}
}

void AsyncCompilerThread::PushFragmentConfig(const PICA::FragmentConfig& config, CachedProgram* cachedProgram) {
	CompilingProgram* newProgram = preallocatedPrograms[preallocatedProgramsIndex];
	newProgram->program = cachedProgram;
	*newProgram->config = config;
	preallocatedProgramsIndex = (preallocatedProgramsIndex + 1) % preallocatedProgramsSize;
	bool pushed = programQueue.Push(newProgram);

	if (!pushed) {
		Helpers::warn("AsyncCompilerThread: Queue full, spinning");

		while (!pushed) {
			pushed = programQueue.Push(newProgram);
		}
	}
}

void AsyncCompilerThread::Finish() {
	hasWork.store(true);

	// Wait for the compiler thread to finish any outstanding work
	while (hasWork.load()) {}
}