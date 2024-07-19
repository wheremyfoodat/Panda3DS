Async shader compilation should hide the problem of compilation stutter by using the ubershader when
specialized shaders are being compiled on a separate thread. To activate this mode, set shaderMode to hybrid
in config.toml

The way it works is the following:

A shaderCompilationThread is started, which holds its own separate OpenGL context.
The communication happens with two lock-free single consumer single producer queues, to avoid messing with mutexes and potential
deadlocks.

The configQueue, a queue that contains PICA::FragmentConfig objects. These objects dictate how a specialized shader should
be compiled.

The compiledQueue, a queue that contains CompiledShader objects which contain the compiled program.

When drawVertices happens, if shaderMode is set to Hybrid, this is what happens:

Emulator thread checks if the shader already exists in shaderCache. Only the emulator thread accesses the shaderCache, so no locks are needed.

If the shader exists and is ready (CachedProgram::ready == true) it uses it like normal.

If the shader exists and isn't ready, that means the shader compilation was queued but the compiler thread hasn't finished yet and sets useUbershader to true.

If the shader doesn't exist, it pushes to configQueue the current PICA::FragmentConfig (by copying it over) and sets useUbershader to true.

Then, at some point, the compilation thread will detect that there's configurations it needs to compile in configQueue.
The compilation thread will compile the shader and then get its binary with glGetProgramBinary. Then it will push this binary to compiledQueue.

On the other end, the emulator on every drawVertices will check if there's stuff in compiledQueue. If there is, it will use
glProgramBinary to convert the raw binary data into the program in shaderCache and set the CachedProgram::ready to true

Then, if the same PICA::FragmentConfig is needed again, it will be available in shaderCache.