#include "PICA/shader_unit.hpp"

void ShaderUnit::reset() {
	vs.reset();
	gs.reset();
}

void PICAShader::reset() {
	loadedShader.fill(0);
	bufferedShader.fill(0);
	operandDescriptors.fill(0);

	intUniforms.fill(0);
	boolUniform = 0;
	bufferIndex = 0;
	floatUniformIndex = 0;
	floatUniformWordCount = 0;
	opDescriptorIndex = 0;
	f32UniformTransfer = false;

	const vec4f zero = vec4f({ f24::zero(), f24::zero(), f24::zero(), f24::zero() });
	attributes.fill(zero);
	floatUniforms.fill(zero);
	outputs.fill(zero);
	tempRegisters.fill(zero);
}