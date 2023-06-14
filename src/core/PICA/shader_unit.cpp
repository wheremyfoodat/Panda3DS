#include "PICA/shader_unit.hpp"
#include "cityhash.hpp"

void ShaderUnit::reset() {
	vs.reset();
	gs.reset();
}

void PICAShader::reset() {
	loadedShader.fill(0);
	bufferedShader.fill(0);
	operandDescriptors.fill(0);

	boolUniform = 0;
	bufferIndex = 0;
	floatUniformIndex = 0;
	floatUniformWordCount = 0;
	opDescriptorIndex = 0;
	f32UniformTransfer = false;

	const vec4f zero = vec4f({ f24::zero(), f24::zero(), f24::zero(), f24::zero() });
	inputs.fill(zero);
	floatUniforms.fill(zero);
	outputs.fill(zero);
	tempRegisters.fill(zero);

	for (auto& e : intUniforms) {
		e.x() = e.y() = e.z() = e.w() = 0;
	}

	addrRegister.x() = 0;
	addrRegister.y() = 0;
	loopCounter = 0;

	codeHashDirty = true;
	opdescHashDirty = true;
}