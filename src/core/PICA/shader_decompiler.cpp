#include "PICA/shader_decompiler.hpp"

#include "config.hpp"

using namespace PICA;
using namespace PICA::ShaderGen;
using Function = ControlFlow::Function;
using ExitMode = Function::ExitMode;

void ControlFlow::analyze(const PICAShader& shader, u32 entrypoint) {
	analysisFailed = false;

	const Function* function = addFunction(entrypoint, PICAShader::maxInstructionCount);
	if (function == nullptr) {
		analysisFailed = true;
	}
}

ExitMode analyzeFunction(u32 start, u32 end, Function::Labels& labels) { return ExitMode::Unknown; }

std::string ShaderDecompiler::decompile() {
	controlFlow.analyze(shader, entrypoint);

	if (controlFlow.analysisFailed) {
		return "";
	}

	decompiledShader = "";

	switch (api) {
		case API::GL: decompiledShader += "#version 410 core"; break;
		case API::GLES: decompiledShader += "#version 300 es"; break;
		default: break;
	}

	if (config.accurateShaderMul) {
		// Safe multiplication handler from Citra: Handles the PICA's 0 * inf = 0 edge case
		decompiledShader += R"(
			vec4 safe_mul(vec4 a, vec4 b) {
				vec4 res = a * b;
				return mix(res, mix(mix(vec4(0.0), res, isnan(rhs)), product, isnan(lhs)), isnan(res));
			}
		)";
	}

	return decompiledShader;
}

std::string PICA::ShaderGen::decompileShader(PICAShader& shader, EmulatorConfig& config, u32 entrypoint, API api, Language language) {
	ShaderDecompiler decompiler(shader, config, entrypoint, api, language);

	return decompiler.decompile();
}