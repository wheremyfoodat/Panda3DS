#include "PICA/shader_decompiler.hpp"

#include "config.hpp"

using namespace PICA;
using namespace PICA::ShaderGen;
using Function = ControlFlow::Function;
using ExitMode = Function::ExitMode;

void ControlFlow::analyze(const PICAShader& shader, u32 entrypoint) {
	analysisFailed = false;

	const Function* function = addFunction(shader, entrypoint, PICAShader::maxInstructionCount);
	if (function == nullptr) {
		analysisFailed = true;
	}
}

ExitMode ControlFlow::analyzeFunction(const PICAShader& shader, u32 start, u32 end, Function::Labels& labels) {
	// Initialize exit mode to unknown by default, in order to detect things like unending loops
	auto [it, inserted] = exitMap.emplace(AddressRange(start, end), ExitMode::Unknown);
	// Function has already been analyzed and is in the map so it wasn't added, don't analyze again
	if (!inserted) {
		return it->second;
	}

	// Make sure not to go out of bounds on the shader
	for (u32 pc = start; pc < PICAShader::maxInstructionCount && pc != end; pc++) {
		const u32 instruction = shader.loadedShader[pc];
		const u32 opcode = instruction >> 26;

		switch (opcode) {
			case ShaderOpcodes::JMPC: Helpers::panic("Unimplemented control flow operation (JMPC)");
			case ShaderOpcodes::JMPU: Helpers::panic("Unimplemented control flow operation (JMPU)");
			case ShaderOpcodes::IFU: Helpers::panic("Unimplemented control flow operation (IFU)");
			case ShaderOpcodes::IFC: Helpers::panic("Unimplemented control flow operation (IFC)");
			case ShaderOpcodes::CALL: Helpers::panic("Unimplemented control flow operation (CALL)");
			case ShaderOpcodes::CALLC: Helpers::panic("Unimplemented control flow operation (CALLC)");
			case ShaderOpcodes::CALLU: Helpers::panic("Unimplemented control flow operation (CALLU)");
			case ShaderOpcodes::LOOP: Helpers::panic("Unimplemented control flow operation (LOOP)");
			case ShaderOpcodes::END: it->second = ExitMode::AlwaysEnd; return it->second;

			default: break;
		}
	}

	// A function without control flow instructions will always reach its "return point" and return
	return ExitMode::AlwaysReturn;
}

void ShaderDecompiler::compileRange(const AddressRange& range) {
	u32 pc = range.start;
	const u32 end = range.end >= range.start ? range.end : PICAShader::maxInstructionCount;
	bool finished = false;

	while (pc < end && !finished) {
		compileInstruction(pc, finished);
	}
}

const Function* ShaderDecompiler::findFunction(const AddressRange& range) {
	for (const Function& func : controlFlow.functions) {
		if (range.start == func.start && range.end == func.end) {
			return &func;
		}
	}

	return nullptr;
}

void ShaderDecompiler::writeAttributes() {
	decompiledShader += R"(
		layout(std140) uniform PICAShaderUniforms {
			vec4 uniform_float[96];
			uvec4 uniform_int;
			uint uniform_bool;
		};
)";

	decompiledShader += "\n";
}

std::string ShaderDecompiler::decompile() {
	controlFlow.analyze(shader, entrypoint);

	if (controlFlow.analysisFailed) {
		return "";
	}

	decompiledShader = "";

	switch (api) {
		case API::GL: decompiledShader += "#version 410 core\n"; break;
		case API::GLES: decompiledShader += "#version 300 es\n"; break;
		default: break;
	}

	writeAttributes();

	if (config.accurateShaderMul) {
		// Safe multiplication handler from Citra: Handles the PICA's 0 * inf = 0 edge case
		decompiledShader += R"(
			vec4 safe_mul(vec4 a, vec4 b) {
				vec4 res = a * b;
				return mix(res, mix(mix(vec4(0.0), res, isnan(rhs)), product, isnan(lhs)), isnan(res));
			}
		)";
	}

	// Forward declare every generated function first so that we can easily call anything from anywhere.
	for (auto& func : controlFlow.functions) {
		decompiledShader += func.getForwardDecl();
	}

	decompiledShader += "void pica_shader_main() {\n";
	AddressRange mainFunctionRange(entrypoint, PICAShader::maxInstructionCount);
	callFunction(*findFunction(mainFunctionRange));
	decompiledShader += "}\n";

	for (auto& func : controlFlow.functions) {
		if (func.outLabels.size() > 0) {
			Helpers::panic("Function with out labels");
		}

		decompiledShader += "void " + func.getIdentifier() + "() {\n";
		compileRange(AddressRange(func.start, func.end));
		decompiledShader += "}\n";
	}

	return decompiledShader;
}

void ShaderDecompiler::compileInstruction(u32& pc, bool& finished) {
	const u32 instruction = shader.loadedShader[pc];
	const u32 opcode = instruction >> 26;

	switch (opcode) {
		case ShaderOpcodes::DP4: decompiledShader += "dp4\n"; break;
		case ShaderOpcodes::MOV: decompiledShader += "mov\n"; break;
		case ShaderOpcodes::END: finished = true; return;
		default: Helpers::warn("GLSL recompiler: Unknown opcode: %X", opcode); break;
	}

	pc++;
}

void ShaderDecompiler::callFunction(const Function& function) { decompiledShader += function.getCallStatement() + ";\n"; }

std::string ShaderGen::decompileShader(PICAShader& shader, EmulatorConfig& config, u32 entrypoint, API api, Language language) {
	ShaderDecompiler decompiler(shader, config, entrypoint, api, language);

	return decompiler.decompile();
}