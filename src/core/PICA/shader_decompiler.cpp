#include "PICA/shader_decompiler.hpp"

#include <fmt/format.h>

#include <array>
#include <cassert>

#include "config.hpp"

using namespace PICA;
using namespace PICA::ShaderGen;
using namespace Helpers;

using Function = ControlFlow::Function;
using ExitMode = Function::ExitMode;

void ControlFlow::analyze(const PICAShader& shader, u32 entrypoint) {
	analysisFailed = false;

	const Function* function = addFunction(shader, entrypoint, PICAShader::maxInstructionCount);
	if (function == nullptr || function->exitMode != ExitMode::AlwaysEnd) {
		analysisFailed = true;
	}
}

// Helpers for merging parallel/series exit methods from Citra
// Merges exit method of two parallel branches.
static ExitMode exitParallel(ExitMode a, ExitMode b) {
	if (a == ExitMode::Unknown) {
		return b;
	}
	else if (b == ExitMode::Unknown) {
		return a;
	}
	else if (a == b) {
		return a;
	}
	return ExitMode::Conditional;
}

// Cascades exit method of two blocks of code.
static ExitMode exitSeries(ExitMode a, ExitMode b) {
	assert(a != ExitMode::AlwaysEnd);

	if (a == ExitMode::Unknown) {
		return ExitMode::Unknown;
	}

	if (a == ExitMode::AlwaysReturn) {
		return b;
	}

	if (b == ExitMode::Unknown || b == ExitMode::AlwaysEnd) {
		return ExitMode::AlwaysEnd;
	}

	return ExitMode::Conditional;
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
			case ShaderOpcodes::JMPC:
			case ShaderOpcodes::JMPU: {
				const u32 dest = getBits<10, 12>(instruction);
				// Register this jump address to our outLabels set
				labels.insert(dest);

				// This opens up 2 parallel paths of execution
				auto branchTakenExit = analyzeFunction(shader, dest, end, labels);
				auto branchNotTakenExit = analyzeFunction(shader, pc + 1, end, labels);
				it->second = exitParallel(branchTakenExit, branchNotTakenExit);
				return it->second;
			}

			case ShaderOpcodes::IFU:
			case ShaderOpcodes::IFC: {
				const u32 num = instruction & 0xff;
				const u32 dest = getBits<10, 12>(instruction);

				const Function* branchTakenFunc = addFunction(shader, pc + 1, dest);
				// Check if analysis of the branch taken func failed and return unknown if it did
				if (analysisFailed) {
					it->second = ExitMode::Unknown;
					return it->second;
				}

				// Next analyze the not taken func
				ExitMode branchNotTakenExitMode = ExitMode::AlwaysReturn;
				if (num != 0) {
					const Function* branchNotTakenFunc = addFunction(shader, dest, dest + num);
					// Check if analysis failed and return unknown if it did
					if (analysisFailed) {
						it->second = ExitMode::Unknown;
						return it->second;
					}

					branchNotTakenExitMode = branchNotTakenFunc->exitMode;
				}

				auto parallel = exitParallel(branchTakenFunc->exitMode, branchNotTakenExitMode);
				// Both branches of the if/else end, so there's nothing after the call
				if (parallel == ExitMode::AlwaysEnd) {
					it->second = parallel;
					return it->second;
				} else {
					ExitMode afterConditional = analyzeFunction(shader, dest + num, end, labels);
					ExitMode conditionalExitMode = exitSeries(parallel, afterConditional);
					it->second = conditionalExitMode;
					return it->second;
				}
				break;
			}

			case ShaderOpcodes::CALL: {
				const u32 num = instruction & 0xff;
				const u32 dest = getBits<10, 12>(instruction);
				const Function* calledFunction = addFunction(shader, dest, dest + num);

				// Check if analysis of the branch taken func failed and return unknown if it did
				if (analysisFailed) {
					it->second = ExitMode::Unknown;
					return it->second;
				}

				if (calledFunction->exitMode == ExitMode::AlwaysEnd) {
					it->second = ExitMode::AlwaysEnd;
					return it->second;
				}

				// Exit mode of the remainder of this function, after we return from the callee
				const ExitMode postCallExitMode = analyzeFunction(shader, pc + 1, end, labels);
				const ExitMode exitMode = exitSeries(calledFunction->exitMode, postCallExitMode);

				it->second = exitMode;
				return exitMode;
			}

			case ShaderOpcodes::CALLC:
			case ShaderOpcodes::CALLU: {
				const u32 num = instruction & 0xff;
				const u32 dest = getBits<10, 12>(instruction);
				const Function* calledFunction = addFunction(shader, dest, dest + num);

				// Check if analysis of the branch taken func failed and return unknown if it did
				if (analysisFailed) {
					it->second = ExitMode::Unknown;
					return it->second;
				}

				// Exit mode of the remainder of this function, after we return from the callee
				const ExitMode postCallExitMode = analyzeFunction(shader, pc + 1, end, labels);
				const ExitMode exitMode = exitSeries(exitParallel(calledFunction->exitMode, ExitMode::AlwaysReturn), postCallExitMode);

				it->second = exitMode;
				return exitMode;
			}

			case ShaderOpcodes::LOOP: {
				u32 dest = getBits<10, 12>(instruction);
				const Function* loopFunction = addFunction(shader, pc + 1, dest + 1);
				if (analysisFailed) {
					it->second = ExitMode::Unknown;
					return it->second;
				}

				if (loopFunction->exitMode == ExitMode::AlwaysEnd) {
					it->second = ExitMode::AlwaysEnd;
					return it->second;
				}

				const ExitMode afterLoop = analyzeFunction(shader, dest + 1, end, labels);
				const ExitMode exitMode = exitSeries(loopFunction->exitMode, afterLoop);
				it->second = exitMode;
				return it->second;
			}

			case ShaderOpcodes::END: it->second = ExitMode::AlwaysEnd; return it->second;
			default: break;
		}
	}

	// A function without control flow instructions will always reach its "return point" and return
	it->second = ExitMode::AlwaysReturn;
	return it->second;
}

std::pair<u32, bool> ShaderDecompiler::compileRange(const AddressRange& range) {
	u32 pc = range.start;
	const u32 end = range.end >= range.start ? range.end : PICAShader::maxInstructionCount;
	bool finished = false;

	while (pc < end && !finished) {
		compileInstruction(pc, finished);
	}

	return std::make_pair(pc, finished);
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
	layout(location = 0) in vec4 inputs[16];
	layout(std140) uniform PICAShaderUniforms {
		vec4 uniform_f[96];
		uvec4 uniform_i;
		uint uniform_bool;
	};

	vec4 temp[16];
	vec4 out_regs[16];
	vec4 dummy_vec = vec4(0.0);
	ivec3 addr_reg = ivec3(0);
	bvec2 cmp_reg = bvec2(false);

	vec4 uniform_indexed(int source, int offset) {
		int clipped_offs = (offset >= -128 && offset <= 127) ? offset : 0;
		uint index = uint(clipped_offs + source) & 127u;
		return (index < 96u) ? uniform_f[index] : vec4(1.0);
	}
)";
}

std::string ShaderDecompiler::decompile() {
	controlFlow.analyze(shader, entrypoint);

	if (controlFlow.analysisFailed) {
		return "";
	}

	compilationError = false;
	decompiledShader = "";

	switch (api) {
		case API::GL: decompiledShader += "#version 410 core\n"; break;
		case API::GLES: decompiledShader += "#version 300 es\nprecision mediump float;\nprecision mediump int;\n"; break;
		default: break;
	}

	writeAttributes();

	if (config.accurateShaderMul) {
		// Safe multiplication handler from Citra: Handles the PICA's 0 * inf = 0 edge case
		decompiledShader += R"(
			vec4 safe_mul(vec4 a, vec4 b) {
				vec4 res = a * b;
				return mix(res, mix(mix(vec4(0.0), res, isnan(b)), res, isnan(a)), isnan(res));
			}
		)";
	}

	// Forward declare every generated function first so that we can easily call anything from anywhere.
	for (auto& func : controlFlow.functions) {
		decompiledShader += func.getForwardDecl();
	}

	decompiledShader += "void pica_shader_main() {\n";
	AddressRange mainFunctionRange(entrypoint, PICAShader::maxInstructionCount);
	auto mainFunc = findFunction(mainFunctionRange);

	decompiledShader += mainFunc->getCallStatement() + ";\n}\n";

	for (const Function& func : controlFlow.functions) {
		if (func.outLabels.empty()) {
			decompiledShader += fmt::format("bool {}() {{\n", func.getIdentifier());

			auto [pc, finished] = compileRange(AddressRange(func.start, func.end));
			if (!finished) {
				decompiledShader += "return false;";
			}

			decompiledShader += "}\n";
		} else {
			auto labels = func.outLabels;
			labels.insert(func.start);

			// If a function has jumps and "labels", this needs to be emulated using a switch-case, with the variable being switched on being the
			// current PC
			decompiledShader += fmt::format("bool {}() {{\n", func.getIdentifier());
			decompiledShader += fmt::format("uint pc = {}u;\n", func.start);
			decompiledShader += "while(true){\nswitch(pc){\n";

			for (u32 label : labels) {
				decompiledShader += fmt::format("case {}u: {{", label);
				// Fetch the next label whose address > label
				auto it = labels.lower_bound(label + 1);
				u32 next = (it == labels.end()) ? func.end : *it;

				auto [endPC, finished] = compileRange(AddressRange(label, next));
				if (endPC > next && !finished) {
					labels.insert(endPC);
					decompiledShader += fmt::format("pc = {}u; break;", endPC);
				}

				// Fallthrough to next label
				decompiledShader += "}\n";
			}

			decompiledShader += "default: return false;\n";
			// Exit the switch and loop
			decompiledShader += "} }\n";

			// Exit the function
			decompiledShader += "return false;\n";
			decompiledShader += "}\n";
		}
	}

	// We allow some leeway for "compilation errors" in addition to control flow errors, in cases where eg an unimplemented instruction
	// or an instruction that we can't emulate in GLSL is found in the instruction stream. Just like control flow errors, these return an empty string
	// and the renderer core will decide to use CPU shaders instead
	if (compilationError) [[unlikely]] {
		return "";
	}

	return decompiledShader;
}

std::string ShaderDecompiler::getSource(u32 source, [[maybe_unused]] u32 index) const {
	if (source < 0x10) {
		return "inputs[" + std::to_string(source) + "]";
	} else if (source < 0x20) {
		return "temp[" + std::to_string(source - 0x10) + "]";
	} else {
		const usize floatIndex = (source - 0x20) & 0x7f;

		if (index == 0) {
			if (floatIndex >= 96) [[unlikely]] {
				return "dummy_vec";
			}
			return "uniform_f[" + std::to_string(floatIndex) + "]";
		} else {
			static constexpr std::array<const char*, 4> offsets = {"0", "addr_reg.x", "addr_reg.y", "addr_reg.z"};
			return fmt::format("uniform_indexed({}, {})", floatIndex, offsets[index]);
		}
	}
}

std::string ShaderDecompiler::getDest(u32 dest) const {
	if (dest < 0x10) {
		return "out_regs[" + std::to_string(dest) + "]";
	} else if (dest < 0x20) {
		return "temp[" + std::to_string(dest - 0x10) + "]";
	} else {
		return "dummy_vec";
	}
}

std::string ShaderDecompiler::getSwizzlePattern(u32 swizzle) const {
	// If the swizzle field is this value then the swizzle pattern is .xyzw so we don't need a shuffle
	static constexpr uint noSwizzle = 0x1B;
	if (swizzle == noSwizzle) {
		return "";
	}

	static constexpr std::array<char, 4> names = {'x', 'y', 'z', 'w'};
	std::string ret(".    ");
	
	for (int i = 0; i < 4; i++) {
		ret[3 - i + 1] = names[swizzle & 0x3];
		swizzle >>= 2;
	}

	return ret;
}

std::string ShaderDecompiler::getDestSwizzle(u32 destinationMask) const {
	std::string ret = ".";
	if (destinationMask & 0b1000) {
		ret += "x";
	}

	if (destinationMask & 0b100) {
		ret += "y";
	}
	
	if (destinationMask & 0b10) {
		ret += "z";
	}
	
	if (destinationMask & 0b1) {
		ret += "w";
	}

	return ret;
}

void ShaderDecompiler::setDest(u32 operandDescriptor, const std::string& dest, const std::string& value) {
	u32 destinationMask = operandDescriptor & 0xF;

	std::string destSwizzle = getDestSwizzle(destinationMask);
	// We subtract 1 for the "." character of the swizzle descriptor
	u32 writtenLaneCount = destSwizzle.size() - 1;

	// All lanes are masked out, so the operation is a nop.
	if (writtenLaneCount == 0) {
		return;
	}

	// Don't write destination swizzle if all lanes are getting written to
	decompiledShader += fmt::format("{}{} = ", dest, writtenLaneCount == 4 ? "" : destSwizzle);
	if (writtenLaneCount <= 3) {
		decompiledShader += fmt::format("({}){};\n", value, destSwizzle);
	} else if (writtenLaneCount == 4) {
		decompiledShader += fmt::format("{};\n", value);
	}
}

void ShaderDecompiler::compileInstruction(u32& pc, bool& finished) {
	const u32 instruction = shader.loadedShader[pc];
	const u32 opcode = instruction >> 26;

	if (usesCommonEncoding(instruction)) {
		const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x7f];
		const bool invertSources = (opcode == ShaderOpcodes::SLTI || opcode == ShaderOpcodes::SGEI || opcode == ShaderOpcodes::DPHI);

		// src1 and src2 indexes depend on whether this is one of the inverting instructions or not
		const u32 src1Index = invertSources ? getBits<14, 5>(instruction) : getBits<12, 7>(instruction);
		const u32 src2Index = invertSources ? getBits<7, 7>(instruction) : getBits<7, 5>(instruction);

		const u32 idx = getBits<19, 2>(instruction);
		const u32 destIndex = getBits<21, 5>(instruction);

		const bool negate1 = (getBit<4>(operandDescriptor)) != 0;
		const u32 swizzle1 = getBits<5, 8>(operandDescriptor);
		const bool negate2 = (getBit<13>(operandDescriptor)) != 0;
		const u32 swizzle2 = getBits<14, 8>(operandDescriptor);

		std::string src1 = negate1 ? "-" : "";
		src1 += getSource(src1Index, invertSources ? 0 : idx);
		src1 += getSwizzlePattern(swizzle1);

		std::string src2 = negate2 ? "-" : "";
		src2 += getSource(src2Index, invertSources ? idx : 0);
		src2 += getSwizzlePattern(swizzle2);

		std::string dest = getDest(destIndex);

		switch (opcode) {
			case ShaderOpcodes::MOV: setDest(operandDescriptor, dest, src1); break;
			case ShaderOpcodes::ADD: setDest(operandDescriptor, dest, fmt::format("{} + {}", src1, src2)); break;
			case ShaderOpcodes::MUL:
				if (!config.accurateShaderMul) {
					setDest(operandDescriptor, dest, fmt::format("{} * {}", src1, src2));
				} else {
					setDest(operandDescriptor, dest, fmt::format("safe_mul({}, {})", src1, src2));
				}
				break;
			case ShaderOpcodes::MAX: setDest(operandDescriptor, dest, fmt::format("max({}, {})", src1, src2)); break;
			case ShaderOpcodes::MIN: setDest(operandDescriptor, dest, fmt::format("min({}, {})", src1, src2)); break;

			case ShaderOpcodes::DP3:
				if (!config.accurateShaderMul) {
					setDest(operandDescriptor, dest, fmt::format("vec4(dot({}.xyz, {}.xyz))", src1, src2));
				} else {
					// A dot product between a and b is equivalent to the per-lane multiplication of a and b followed by a dot product with vec3(1.0)
					setDest(operandDescriptor, dest, fmt::format("vec4(dot(safe_mul({}, {}).xyz, vec3(1.0)))", src1, src2));
				}
				break;
			case ShaderOpcodes::DP4:
				if (!config.accurateShaderMul) {
					setDest(operandDescriptor, dest, fmt::format("vec4(dot({}, {}))", src1, src2));
				} else {
					// A dot product between a and b is equivalent to the per-lane multiplication of a and b followed by a dot product with vec4(1.0)
					setDest(operandDescriptor, dest, fmt::format("vec4(dot(safe_mul({}, {}), vec4(1.0)))", src1, src2));
				}
				break;
			case ShaderOpcodes::FLR: setDest(operandDescriptor, dest, fmt::format("floor({})", src1)); break;
			case ShaderOpcodes::RSQ: setDest(operandDescriptor, dest, fmt::format("vec4(inversesqrt({}.x))", src1)); break;
			case ShaderOpcodes::RCP: setDest(operandDescriptor, dest, fmt::format("vec4(1.0 / {}.x)", src1)); break;
			case ShaderOpcodes::LG2: setDest(operandDescriptor, dest, fmt::format("vec4(log2({}.x))", src1)); break;
			case ShaderOpcodes::EX2: setDest(operandDescriptor, dest, fmt::format("vec4(exp2({}.x))", src1)); break;

			case ShaderOpcodes::SLT:
			case ShaderOpcodes::SLTI: setDest(operandDescriptor, dest, fmt::format("vec4(lessThan({}, {}))", src1, src2)); break;

			case ShaderOpcodes::SGE:
			case ShaderOpcodes::SGEI: setDest(operandDescriptor, dest, fmt::format("vec4(greaterThanEqual({}, {}))", src1, src2)); break;

			case ShaderOpcodes::DPH:
			case ShaderOpcodes::DPHI:
				if (!config.accurateShaderMul) {
					setDest(operandDescriptor, dest, fmt::format("vec4(dot(vec4({}.xyz, 1.0), {}))", src1, src2));
				} else {
					// A dot product between a and b is equivalent to the per-lane multiplication of a and b followed by a dot product with vec4(1.0)
					setDest(operandDescriptor, dest, fmt::format("vec4(dot(safe_mul(vec4({}.xyz, 1.0), {}), vec4(1.0)))", src1, src2));
				}
				break;

			case ShaderOpcodes::CMP1:
			case ShaderOpcodes::CMP2: {
				static constexpr std::array<const char*, 8> operators = {
					// The last 2 operators always return true and are handled specially
					"==", "!=", "<", "<=", ">", ">=", "", "",
				};

				const u32 cmpY = getBits<21, 3>(instruction);
				const u32 cmpX = getBits<24, 3>(instruction);

				// Compare x first
				if (cmpX >= 6) {
					decompiledShader += "cmp_reg.x = true;\n";
				} else {
					decompiledShader += fmt::format("cmp_reg.x = {}.x {} {}.x;\n", src1, operators[cmpX], src2);
				}

				// Then compare Y
				if (cmpY >= 6) {
					decompiledShader += "cmp_reg.y = true;\n";
				} else {
					decompiledShader += fmt::format("cmp_reg.y = {}.y {} {}.y;\n", src1, operators[cmpY], src2);
				}
				break;
			}

			case ShaderOpcodes::MOVA: {
				const bool writeX = getBit<3>(operandDescriptor);  // Should we write the x component of the address register?
				const bool writeY = getBit<2>(operandDescriptor);

				if (writeX && writeY) {
					decompiledShader += fmt::format("addr_reg.xy = ivec2({}.xy);\n", src1);
				} else if (writeX) {
					decompiledShader += fmt::format("addr_reg.x = int({}.x);\n", src1);
				} else if (writeY) {
					decompiledShader += fmt::format("addr_reg.y = int({}.y);\n", src1);
				}
				break;
			}

			default:
				Helpers::warn("GLSL recompiler: Unknown common opcode: %02X. Falling back to CPU shaders", opcode);
				compilationError = true;
				break;
		}
	} else if (opcode >= 0x30 && opcode <= 0x3F) { // MAD and MADI
		const u32 operandDescriptor = shader.operandDescriptors[instruction & 0x1f];
		const bool isMADI = getBit<29>(instruction) == 0; // We detect MADI based on bit 29 of the instruction

		// src1 and src2 indexes depend on whether this is one of the inverting instructions or not
		const u32 src1Index = getBits<17, 5>(instruction);
		const u32 src2Index = isMADI ? getBits<12, 5>(instruction) : getBits<10, 7>(instruction);
		const u32 src3Index = isMADI ? getBits<5, 7>(instruction) : getBits<5, 5>(instruction);
		const u32 idx = getBits<22, 2>(instruction);
		const u32 destIndex = getBits<24, 5>(instruction);

		const bool negate1 = (getBit<4>(operandDescriptor)) != 0;
		const u32 swizzle1 = getBits<5, 8>(operandDescriptor);
		const bool negate2 = (getBit<13>(operandDescriptor)) != 0;
		const u32 swizzle2 = getBits<14, 8>(operandDescriptor);

		const bool negate3 = (getBit<22>(operandDescriptor)) != 0;
		const u32 swizzle3 = getBits<23, 8>(operandDescriptor);

		std::string src1 = negate1 ? "-" : "";
		src1 += getSource(src1Index, 0);
		src1 += getSwizzlePattern(swizzle1);

		std::string src2 = negate2 ? "-" : "";
		src2 += getSource(src2Index, isMADI ? 0 : idx);
		src2 += getSwizzlePattern(swizzle2);

		std::string src3 = negate3 ? "-" : "";
		src3 += getSource(src3Index, isMADI ? idx : 0);
		src3 += getSwizzlePattern(swizzle3);

		std::string dest = getDest(destIndex);
		if (!config.accurateShaderMul) {
			setDest(operandDescriptor, dest, fmt::format("{} * {} + {}", src1, src2, src3));
		} else {
			setDest(operandDescriptor, dest, fmt::format("safe_mul({}, {}) + {}", src1, src2, src3));
		}
	} else {
		switch (opcode) {
			case ShaderOpcodes::JMPC: {
				const u32 dest = getBits<10, 12>(instruction);
				const u32 condOp = getBits<22, 2>(instruction);
				const uint refY = getBit<24>(instruction);
				const uint refX = getBit<25>(instruction);
				const char* condition = getCondition(condOp, refX, refY);

				decompiledShader += fmt::format("if ({}) {{ pc = {}u; break; }}\n", condition, dest);
				break;
			}

			case ShaderOpcodes::JMPU: {
				const u32 dest = getBits<10, 12>(instruction);
				const u32 bit = getBits<22, 4>(instruction);  // Bit of the bool uniform to check
				const u32 mask = 1u << bit;
				const u32 test = (instruction & 1) ^ 1;  // If the LSB is 0 we jump if bit = 1, otherwise 0

				decompiledShader += fmt::format("if ((uniform_bool & {}u) {} 0u) {{ pc = {}u; break; }}\n", mask, (test != 0) ? "!=" : "==", dest);
				break;
			}

			case ShaderOpcodes::IFU:
			case ShaderOpcodes::IFC: {
				const u32 num = instruction & 0xff;
				const u32 dest = getBits<10, 12>(instruction);
				const Function* conditionalFunc = findFunction(AddressRange(pc + 1, dest));

				if (opcode == ShaderOpcodes::IFC) {
					const u32 condOp = getBits<22, 2>(instruction);
					const uint refY = getBit<24>(instruction);
					const uint refX = getBit<25>(instruction);
					const char* condition = getCondition(condOp, refX, refY);

					decompiledShader += fmt::format("if ({}) {{", condition);
				} else {
					const u32 bit = getBits<22, 4>(instruction);  // Bit of the bool uniform to check
					const u32 mask = 1u << bit;

					decompiledShader += fmt::format("if ((uniform_bool & {}u) != 0u) {{", mask);
				}

				callFunction(*conditionalFunc);
				decompiledShader += "}\n";

				pc = dest;
				if (num > 0) {
					const Function* elseFunc = findFunction(AddressRange(dest, dest + num));
					pc = dest + num;

					decompiledShader += "else { ";
					callFunction(*elseFunc);
					decompiledShader += "}\n";

					if (conditionalFunc->exitMode == ExitMode::AlwaysEnd && elseFunc->exitMode == ExitMode::AlwaysEnd) {
						finished = true;
						return;
					}
				}

				return;
			}

			case ShaderOpcodes::CALL:
			case ShaderOpcodes::CALLC:
			case ShaderOpcodes::CALLU: {
				const u32 num = instruction & 0xff;
				const u32 dest = getBits<10, 12>(instruction);
				const Function* calledFunc = findFunction(AddressRange(dest, dest + num));

				// Handle conditions for CALLC/CALLU
				if (opcode == ShaderOpcodes::CALLC) {
					const u32 condOp = getBits<22, 2>(instruction);
					const uint refY = getBit<24>(instruction);
					const uint refX = getBit<25>(instruction);
					const char* condition = getCondition(condOp, refX, refY);

					decompiledShader += fmt::format("if ({}) {{", condition);
				} else if (opcode == ShaderOpcodes::CALLU) {
					const u32 bit = getBits<22, 4>(instruction);  // Bit of the bool uniform to check
					const u32 mask = 1u << bit;

					decompiledShader += fmt::format("if ((uniform_bool & {}u) != 0u) {{", mask);
				}

				callFunction(*calledFunc);

				// Close brackets for CALLC/CALLU
				if (opcode != ShaderOpcodes::CALL) {
					decompiledShader += "}";
				}

				if (opcode == ShaderOpcodes::CALL && calledFunc->exitMode == ExitMode::AlwaysEnd) {
					finished = true;
					return;
				}
				break;
			}

			case ShaderOpcodes::LOOP: {
				const u32 dest = getBits<10, 12>(instruction);
				const u32 uniformIndex = getBits<22, 2>(instruction);

				// loop counter = uniform.y
				decompiledShader += fmt::format("addr_reg.z = int((uniform_i[{}] >> 8u) & 0xFFu);\n", uniformIndex);
				decompiledShader += fmt::format(
					"for (uint loopCtr{} = 0u; loopCtr{} <= (uniform_i[{}] & 0xFFu); loopCtr{}++, addr_reg.z += int((uniform_i[{}] >> "
					"16u) & 0xFFu)) {{\n",
					pc, pc, uniformIndex, pc, uniformIndex
				);

				AddressRange range(pc + 1, dest + 1);
				const Function* func = findFunction(range);
				callFunction(*func);
				decompiledShader += "}\n";

				// Jump to the end of the loop. We don't want to compile the code inside the loop again.
				// This will be incremented by 1 due to the pc++ at the end of this loop.
				pc = dest;

				if (func->exitMode == ExitMode::AlwaysEnd) {
					finished = true;
					return;
				}
				break;
			}

			case ShaderOpcodes::END:
				decompiledShader += "return true;\n";
				finished = true;
				return;

			case ShaderOpcodes::NOP: break;

			default:
				Helpers::warn("GLSL recompiler: Unknown opcode: %02X. Falling back to CPU shaders", opcode);
				compilationError = true;
				break;
		}
	}

	pc++;
}

bool ShaderDecompiler::usesCommonEncoding(u32 instruction) const {
	const u32 opcode = instruction >> 26;
	switch (opcode) {
		case ShaderOpcodes::ADD:
		case ShaderOpcodes::CMP1:
		case ShaderOpcodes::CMP2:
		case ShaderOpcodes::MUL:
		case ShaderOpcodes::MIN:
		case ShaderOpcodes::MAX:
		case ShaderOpcodes::FLR:
		case ShaderOpcodes::DP3:
		case ShaderOpcodes::DP4:
		case ShaderOpcodes::DPH:
		case ShaderOpcodes::DPHI:
		case ShaderOpcodes::LG2:
		case ShaderOpcodes::EX2:
		case ShaderOpcodes::RCP:
		case ShaderOpcodes::RSQ:
		case ShaderOpcodes::MOV:
		case ShaderOpcodes::MOVA:
		case ShaderOpcodes::SLT:
		case ShaderOpcodes::SLTI:
		case ShaderOpcodes::SGE:
		case ShaderOpcodes::SGEI:
		case ShaderOpcodes::LITP: return true;

		default: return false;
	}
}

void ShaderDecompiler::callFunction(const Function& function) {
	switch (function.exitMode) {
		// This function always ends, so call it and return true to signal that we're gonna be ending the shader
		case ExitMode::AlwaysEnd: decompiledShader += function.getCallStatement() + ";\nreturn true;\n"; break;
		// This function will potentially end. Call it, see if it returns that it ended, and return that we're ending if it did
		case ExitMode::Conditional: decompiledShader += fmt::format("if ({}) {{ return true; }}\n", function.getCallStatement()); break;
		// This function will not end. Just call it like a normal function.
		default: decompiledShader += function.getCallStatement() + ";\n"; break;
	}
}

std::string ShaderGen::decompileShader(PICAShader& shader, EmulatorConfig& config, u32 entrypoint, API api, Language language) {
	ShaderDecompiler decompiler(shader, config, entrypoint, api, language);

	return decompiler.decompile();
}

const char* ShaderDecompiler::getCondition(u32 cond, u32 refX, u32 refY) {
	static constexpr std::array<const char*, 16> conditions = {
		// ref(Y, X) = (0, 0)
		"!all(cmp_reg)",
		"all(not(cmp_reg))",
		"!cmp_reg.x",
		"!cmp_reg.y",

		// ref(Y, X) = (0, 1)
		"cmp_reg.x || !cmp_reg.y",
		"cmp_reg.x && !cmp_reg.y",
		"cmp_reg.x",
		"!cmp_reg.y",

		// ref(Y, X) = (1, 0)
		"!cmp_reg.x || cmp_reg.y",
		"!cmp_reg.x && cmp_reg.y",
		"!cmp_reg.x",
		"cmp_reg.y",

		// ref(Y, X) = (1, 1)
		"any(cmp_reg)",
		"all(cmp_reg)",
		"cmp_reg.x",
		"cmp_reg.y",
	};
	const u32 key = (cond & 0b11) | (refX << 2) | (refY << 3);

	return conditions[key];
}
