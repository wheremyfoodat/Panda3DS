#pragma once
#include <string>

#include "PICA/gpu.hpp"
#include "PICA/pica_frag_config.hpp"
#include "PICA/pica_vert_config.hpp"
#include "PICA/regs.hpp"
#include "PICA/shader_gen_types.hpp"
#include "helpers.hpp"

namespace PICA::ShaderGen {
	class FragmentGenerator {
		API api;
		Language language;

		void compileTEV(std::string& shader, int stage, const PICA::FragmentConfig& config);
		void getSource(std::string& shader, PICA::TexEnvConfig::Source source, int index, const PICA::FragmentConfig& config);
		void getColorOperand(std::string& shader, PICA::TexEnvConfig::Source source, PICA::TexEnvConfig::ColorOperand color, int index, const PICA::FragmentConfig& config);
		void getAlphaOperand(std::string& shader, PICA::TexEnvConfig::Source source, PICA::TexEnvConfig::AlphaOperand alpha, int index, const PICA::FragmentConfig& config);
		void getColorOperation(std::string& shader, PICA::TexEnvConfig::Operation op);
		void getAlphaOperation(std::string& shader, PICA::TexEnvConfig::Operation op);

		void applyAlphaTest(std::string& shader, const PICA::FragmentConfig& config);
		void compileLights(std::string& shader, const PICA::FragmentConfig& config);
		void compileLUTLookup(std::string& shader, const PICA::FragmentConfig& config, u32 lightIndex, u32 lutID);
		bool isSamplerEnabled(u32 environmentID, u32 lutID);

		void compileFog(std::string& shader, const PICA::FragmentConfig& config);
		void compileLogicOps(std::string& shader, const PICA::FragmentConfig& config);

	  public:
		FragmentGenerator(API api, Language language) : api(api), language(language) {}
		std::string generate(const PICA::FragmentConfig& config, void* driverInfo = nullptr);
		std::string getDefaultVertexShader();
		// For when PICA shader is acceleration is enabled. Turn the PICA shader source into a proper vertex shader
		std::string getVertexShaderAccelerated(const std::string& picaSource, const PICA::VertConfig& vertConfig, bool usingUbershader);

		void setTarget(API api, Language language) {
			this->api = api;
			this->language = language;
		}
	};
};  // namespace PICA::ShaderGen