#pragma once
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "PICA/shader.hpp"
#include "PICA/shader_gen_types.hpp"

struct EmulatorConfig;

namespace PICA::ShaderGen {
	// Control flow analysis is partially based on
	// https://github.com/PabloMK7/citra/blob/d0179559466ff09731d74474322ee880fbb44b00/src/video_core/shader/generator/glsl_shader_decompiler.cpp#L33
	struct ControlFlow {
		struct Function {
			using Labels = std::set<u32>;

			enum class ExitMode {
				Unknown,       // Can't guarantee whether we'll exit properly, fall back to CPU shaders (can happen with jmp shenanigans)
				AlwaysReturn,  // All paths reach the return point.
				Conditional,   // One or more code paths reach the return point or an END instruction conditionally.
				AlwaysEnd,     // All paths reach an END instruction.
			};

			u32 start;           // Starting PC of the function
			u32 end;             // End PC of the function
			Labels outLabels{};  // Labels this function can "goto" (jump) to
			ExitMode exitMode = ExitMode::Unknown;

			explicit Function(u32 start, u32 end) : start(start), end(end) {}
			// Use lexicographic comparison for functions in order to sort them in a set
			bool operator<(const Function& other) const { return std::tie(start, end) < std::tie(other.start, other.end); }
		};

		std::set<Function> functions{};

		// Tells us whether analysis of the shader we're trying to compile failed, in which case we'll need to fail back to shader emulation
		// On the CPU
		bool analysisFailed = false;

		void analyze(const PICAShader& shader, u32 entrypoint);

		// This will recursively add all functions called by the function too, as analyzeFunction will call addFunction on control flow instructions
		const Function* addFunction(u32 start, u32 end) {
			auto searchIterator = functions.find(Function(start, end));
			if (searchIterator != functions.end()) {
				return &(*searchIterator);
			}

			// Add this function and analyze it if it doesn't already exist
			Function function(start, end);
			function.exitMode = analyzeFunction(start, end, function.outLabels);
			
			// This function 
			if (function.exitMode == Function::ExitMode::Unknown) {
				analysisFailed = true;
				return nullptr;
			}

			// Add function to our function list
			auto [it, added] = functions.insert(std::move(function));
			return &(*it);
		}

		Function::ExitMode analyzeFunction(u32 start, u32 end, Function::Labels& labels);
	};

	class ShaderDecompiler {
		ControlFlow controlFlow{};

		PICAShader& shader;
		EmulatorConfig& config;
		std::string decompiledShader;

		u32 entrypoint;
		u32 currentPC;

		API api;
		Language language;

	  public:
		ShaderDecompiler(PICAShader& shader, EmulatorConfig& config, u32 entrypoint, API api, Language language)
			: shader(shader), entrypoint(entrypoint), currentPC(entrypoint), config(config), api(api), language(language), decompiledShader("") {}

		std::string decompile();
	};

	std::string decompileShader(PICAShader& shader, EmulatorConfig& config, u32 entrypoint, API api, Language language);
}  // namespace PICA::ShaderGen