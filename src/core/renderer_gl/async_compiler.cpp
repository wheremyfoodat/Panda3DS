#include "renderer_gl/async_compiler.hpp"
#include "PICA/pica_frag_config.hpp"
#include "PICA/shader_gen.hpp"
#include "glad/gl.h"
#include "opengl.hpp"

namespace AsyncCompiler {
    void* createContext(void* userdata);
    void destroyContext(void* userdata, void* context);
}

AsyncCompilerState::AsyncCompilerState(PICA::ShaderGen::FragmentGenerator& fragShaderGenRef, void* contextCreationUserdata)
    : fragShaderGen(fragShaderGenRef), contextCreationUserdata(contextCreationUserdata)
{
    Start();
}

AsyncCompilerState::~AsyncCompilerState()
{
    Stop();
}

bool AsyncCompilerState::PushFragmentConfig(const PICA::FragmentConfig& config)
{
    PICA::FragmentConfig* newConfig = new PICA::FragmentConfig(config);
    bool pushedSuccessfully = configQueue.Push(newConfig);

    if (!pushedSuccessfully) {
        Helpers::panic("Hlep we managed to fill the shader queue");
    }

    return pushedSuccessfully;
}

bool AsyncCompilerState::PopCompiledProgram(CompiledProgram*& program)
{
    bool hasItem = compiledQueue.Pop(program);
    return hasItem;
}

void AsyncCompilerState::Start() {
    shaderCompilationThread = std::thread([this]() {
        void* context = AsyncCompiler::createContext(contextCreationUserdata);
        if (!context) {
            Helpers::panic("Failed to create async compiler context");
        }

        printf("Async compiler started, version: %s\n", glGetString(GL_VERSION));
        std::string defaultShadergenVSSource = fragShaderGen.getDefaultVertexShader();
	    defaultShadergenVs.create({defaultShadergenVSSource.c_str(), defaultShadergenVSSource.size()}, OpenGL::Vertex);
        
        running = true;

        while (running.load(std::memory_order_relaxed)) {
            PICA::FragmentConfig* fsConfig;
            if (configQueue.Pop(fsConfig)) {
                OpenGL::Program glProgram;
                std::string fs = fragShaderGen.generate(*fsConfig);
                OpenGL::Shader fragShader({fs.c_str(), fs.size()}, OpenGL::Fragment);
		        glProgram.create({defaultShadergenVs, fragShader});

                CompiledProgram* program = new CompiledProgram(*fsConfig);
                GLint size;
                glGetProgramiv(glProgram.handle(), GL_PROGRAM_BINARY_LENGTH, &size);

                if (size == 0) {
                    Helpers::panic("Failed to get program binary size");
                }

                program->binary.resize(size);

                GLint bytesWritten;
                glGetProgramBinary(glProgram.handle(), size, &bytesWritten, &program->binaryFormat, program->binary.data());

                if (bytesWritten != size || bytesWritten == 0) {
                    Helpers::panic("Failed to get program binary");
                }
                
                delete fsConfig;
                
                bool pushedSuccessfully = compiledQueue.Push(program);
                if (!pushedSuccessfully) {
                    Helpers::panic("Hlep we managed to fill the shader queue");
                }
            }

            std::this_thread::yield();
        }

        AsyncCompiler::destroyContext(contextCreationUserdata, context);
    });
}

void AsyncCompilerState::Stop() {
    running = false;
    shaderCompilationThread.join();

    bool hasItem = false;
    do
    {
        CompiledProgram* program;
        hasItem = compiledQueue.Pop(program);
        if (hasItem)
        {
            delete program;
        }
    } while (hasItem);

    do
    {
        PICA::FragmentConfig* config;
        hasItem = configQueue.Pop(config);
        if (hasItem)
        {
            delete config;
        }
    } while (hasItem);
}