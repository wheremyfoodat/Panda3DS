#pragma once
#include <string>

#include "PICA/gpu.hpp"
#include "PICA/pica_frag_config.hpp"
#include "PICA/regs.hpp"
#include "helpers.hpp"

namespace PICA::ShaderGen {
	// Graphics API this shader is targetting
	enum class API { GL, GLES, Vulkan };

	// Shading language to use (Only GLSL for the time being)
	enum class Language { GLSL };

	class FragmentGenerator {
		using PICARegs = std::array<u32, 0x300>;
		API api;
		Language language;

		void compileTEV(std::string& shader, int stage, const PICARegs& regs);
		void getSource(std::string& shader, PICA::TexEnvConfig::Source source, int index);
		void getColorOperand(std::string& shader, PICA::TexEnvConfig::Source source, PICA::TexEnvConfig::ColorOperand color, int index);
		void getAlphaOperand(std::string& shader, PICA::TexEnvConfig::Source source, PICA::TexEnvConfig::AlphaOperand alpha, int index);
		void getColorOperation(std::string& shader, PICA::TexEnvConfig::Operation op);
		void getAlphaOperation(std::string& shader, PICA::TexEnvConfig::Operation op);

		void applyAlphaTest(std::string& shader, const PICARegs& regs);

		u32 textureConfig = 0;

	  public:
		FragmentGenerator(API api, Language language) : api(api), language(language) {}
		std::string generate(const PICARegs& regs, const PICA::FragmentConfig& config);
		std::string getVertexShader(const PICARegs& regs);

		void setTarget(API api, Language language) {
			this->api = api;
			this->language = language;
		}
	};
};  // namespace PICA::ShaderGen