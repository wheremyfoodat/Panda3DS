// For asynchronous shader compilation (hybrid mode)
// See docs/3ds/async_shader_compilation.md for more info
#pragma once

#include <atomic>
#include <vector>
#include <thread>

#include "helpers.hpp"
#include "opengl.hpp"
#include "PICA/pica_frag_config.hpp"
#include "spsc/queue.hpp"

// We make the assumption that not more than 256 shaders will be queued at once.
// This may seem like a hack, but it's a reasonable assumption and it allows us to not have
// to implement a waiting mechanism (wait until the queue is not full)
// In the unlikely event that either queue is full, a panic will be triggered.
constexpr size_t maxInFlightShaderCount = 256;

// A compiled program, ready to be passed to glProgramBinary
struct CompiledProgram
{
    CompiledProgram(const PICA::FragmentConfig& fsConfig)
        : fsConfig(fsConfig)
    {}

    PICA::FragmentConfig fsConfig;
	std::vector<u8> binary;
	u32 binaryFormat;
};

namespace PICA
{
    namespace ShaderGen
    {
        class FragmentGenerator;
    }
};

struct AsyncCompilerState
{
    // The constructor will start the thread that will compile the shaders and create an OpenGL context
    explicit AsyncCompilerState(PICA::ShaderGen::FragmentGenerator& fragShaderGen);

    // The destructor will first set the stop flag and join the thread (which will wait until it exits)
    // and then clean up the queues
    ~AsyncCompilerState();

    // Called from the emulator thread to queue a fragment configuration for compilation
    // Returns false if the queue is full, true otherwise
    bool PushFragmentConfig(const PICA::FragmentConfig& config);

    // Called from the emulator thread to get a compiled program
    // Returns true if a compiled program is available, false otherwise
    bool PopCompiledProgram(CompiledProgram*& program);

    // Manually starts the thread
    void Start();

    // Manually stops the thread
    void Stop();
private:
    void createGLContext();
    void destroyGLContext();

    PICA::ShaderGen::FragmentGenerator& fragShaderGen;

    OpenGL::Shader defaultShadergenVs;

    // Pointers are used in these queues because the lock-free queue require trivial types
	lockfree::spsc::Queue<PICA::FragmentConfig*, maxInFlightShaderCount> configQueue;
	lockfree::spsc::Queue<CompiledProgram*, maxInFlightShaderCount> compiledQueue;
	std::atomic_bool running = false;
	std::thread shaderCompilationThread;
};