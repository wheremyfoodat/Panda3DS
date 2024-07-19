#pragma once
#include <array>
#include <cstring>
#include <type_traits>
#include <unordered_map>

#include "PICA/pica_hash.hpp"
#include "PICA/regs.hpp"
#include "bitfield.hpp"
#include "helpers.hpp"

namespace PICA {
	struct OutputConfig {
		union {
			u32 raw;
			// Merge the enable + compare function into 1 field to avoid duplicate shaders
			// enable == off means a CompareFunction of Always
			BitField<0, 3, CompareFunction> alphaTestFunction;
			BitField<4, 1, u32> depthMapEnable;
		};
	};

	struct TextureConfig {
		u32 texUnitConfig;
		u32 texEnvUpdateBuffer;

		// There's 6 TEV stages, and each one is configured via 4 word-sized registers
		// (+ the constant color register, which we don't include here, otherwise we'd generate too many shaders)
		std::array<u32, 4 * 6> tevConfigs;
	};

	struct Light {
		union {
			u16 raw;
			BitField<0, 3, u16> num;
			BitField<3, 1, u16> directional;
			BitField<4, 1, u16> twoSidedDiffuse;
			BitField<5, 1, u16> distanceAttenuationEnable;
			BitField<6, 1, u16> spotAttenuationEnable;
			BitField<7, 1, u16> geometricFactor0;
			BitField<8, 1, u16> geometricFactor1;
			BitField<9, 1, u16> shadowEnable;
		};
	};

	struct LightingLUTConfig {
		union {
			u32 raw;
			BitField<0, 1, u32> enable;
			BitField<1, 1, u32> absInput;
			BitField<2, 3, u32> type;
		};
		float scale;
	};

	struct LightingConfig {
		union {
			u32 raw{};
			BitField<0, 1, u32> enable;
			BitField<1, 4, u32> lightNum;
			BitField<5, 2, u32> bumpMode;
			BitField<7, 2, u32> bumpSelector;
			BitField<9, 1, u32> bumpRenorm;
			BitField<10, 1, u32> clampHighlights;
			BitField<11, 4, u32> config;
			BitField<15, 1, u32> enablePrimaryAlpha;
			BitField<16, 1, u32> enableSecondaryAlpha;
			BitField<17, 1, u32> enableShadow;
			BitField<18, 1, u32> shadowPrimary;
			BitField<19, 1, u32> shadowSecondary;
			BitField<20, 1, u32> shadowInvert;
			BitField<21, 1, u32> shadowAlpha;
			BitField<22, 2, u32> shadowSelector;
		};

		LightingLUTConfig d0{};
		LightingLUTConfig d1{};
		LightingLUTConfig sp{};
		LightingLUTConfig fr{};
		LightingLUTConfig rr{};
		LightingLUTConfig rg{};
		LightingLUTConfig rb{};

		u32 config1;
		u32 lutAbs;
		u32 lutScale;
		u32 lutSelect;

		std::array<Light, 8> lights{};

		LightingConfig(const std::array<u32, 0x300>& regs) {
			// Ignore lighting registers if it's disabled
			if ((regs[InternalRegs::LightingEnable] & 1) == 0) {
				return;
			}

			const u32 config0 = regs[InternalRegs::LightConfig0];
			const u32 totalLightCount = Helpers::getBits<0, 3>(regs[InternalRegs::LightNumber]) + 1;

			config1 = regs[InternalRegs::LightConfig1];
			lutAbs = regs[InternalRegs::LightLUTAbs];
			lutScale = regs[InternalRegs::LightLUTScale];
			lutSelect = regs[InternalRegs::LightLUTSelect];

			enable = 1;
			lightNum = totalLightCount;

			enableShadow = Helpers::getBit<0>(config0);
			if (enableShadow) [[unlikely]] {
				shadowPrimary = Helpers::getBit<16>(config0);
				shadowSecondary = Helpers::getBit<17>(config0);
				shadowInvert = Helpers::getBit<18>(config0);
				shadowAlpha = Helpers::getBit<19>(config0);
				shadowSelector = Helpers::getBits<24, 2>(config0);
			}

			enablePrimaryAlpha = Helpers::getBit<2>(config0);
			enableSecondaryAlpha = Helpers::getBit<3>(config0);
			config = Helpers::getBits<4, 4>(config0);

			bumpSelector = Helpers::getBits<22, 2>(config0);
			clampHighlights = Helpers::getBit<27>(config0);
			bumpMode = Helpers::getBits<28, 2>(config0);
			bumpRenorm = Helpers::getBit<30>(config0) ^ 1; // 0 = enable so flip it with xor

			for (int i = 0; i < totalLightCount; i++) {
				auto& light = lights[i];
				const u32 lightConfig = regs[InternalRegs::Light0Config + 0x10 * i];

				light.num = (regs[InternalRegs::LightPermutation] >> (i * 4)) & 0x7;
				light.directional = Helpers::getBit<0>(lightConfig);
				light.twoSidedDiffuse = Helpers::getBit<1>(lightConfig);
				light.geometricFactor0 = Helpers::getBit<2>(lightConfig);
				light.geometricFactor1 = Helpers::getBit<3>(lightConfig);

				light.shadowEnable = ((config1 >> i) & 1) ^ 1;                      // This also does 0 = enabled
				light.spotAttenuationEnable = ((config1 >> (8 + i)) & 1) ^ 1;       // Same here
				light.distanceAttenuationEnable = ((config1 >> (24 + i)) & 1) ^ 1;  // Of course same here
			}

			d0.enable = Helpers::getBit<16>(config1) == 0;
			d1.enable = Helpers::getBit<17>(config1) == 0;
			fr.enable = Helpers::getBit<19>(config1) == 0;
			rb.enable = Helpers::getBit<20>(config1) == 0;
			rg.enable = Helpers::getBit<21>(config1) == 0;
			rr.enable = Helpers::getBit<22>(config1) == 0;
			sp.enable = 1;

			const u32 lutAbs = regs[InternalRegs::LightLUTAbs];
			const u32 lutSelect = regs[InternalRegs::LightLUTSelect];
			const u32 lutScale = regs[InternalRegs::LightLUTScale];
			static constexpr float scales[] = {1.0f, 2.0f, 4.0f, 8.0f, 0.0f, 0.0f, 0.25f, 0.5f};

			if (d0.enable) {
				d0.absInput = Helpers::getBit<1>(lutAbs) == 0;
				d0.type = Helpers::getBits<0, 3>(lutSelect);	
				d0.scale = scales[Helpers::getBits<0, 3>(lutScale)];
			}

			if (d1.enable) {
				d1.absInput = Helpers::getBit<5>(lutAbs) == 0;
				d1.type = Helpers::getBits<4, 3>(lutSelect);
				d1.scale = scales[Helpers::getBits<4, 3>(lutScale)];
			}

			sp.absInput = Helpers::getBit<9>(lutAbs) == 0;
			sp.type = Helpers::getBits<8, 3>(lutSelect);
			sp.scale = scales[Helpers::getBits<8, 3>(lutScale)];

			if (fr.enable) {
				fr.absInput = Helpers::getBit<13>(lutAbs) == 0;
				fr.type = Helpers::getBits<12, 3>(lutSelect);
				fr.scale = scales[Helpers::getBits<12, 3>(lutScale)];
			}

			if (rb.enable) {
				rb.absInput = Helpers::getBit<17>(lutAbs) == 0;
				rb.type = Helpers::getBits<16, 3>(lutSelect);
				rb.scale = scales[Helpers::getBits<16, 3>(lutScale)];
			}

			if (rg.enable) {
				rg.absInput = Helpers::getBit<21>(lutAbs) == 0;
				rg.type = Helpers::getBits<20, 3>(lutSelect);
				rg.scale = scales[Helpers::getBits<20, 3>(lutScale)];
			}

			if (rr.enable) {
				rr.absInput = Helpers::getBit<25>(lutAbs) == 0;
				rr.type = Helpers::getBits<24, 3>(lutSelect);
				rr.scale = scales[Helpers::getBits<24, 3>(lutScale)];
			}
		}
	};

	// Config used for identifying unique fragment pipeline configurations
	struct FragmentConfig {
		OutputConfig outConfig;
		TextureConfig texConfig;
		LightingConfig lighting;

		bool operator==(const FragmentConfig& config) const {
			// Hash function and equality operator required by std::unordered_map
			return std::memcmp(this, &config, sizeof(FragmentConfig)) == 0;
		}

		FragmentConfig(const std::array<u32, 0x300>& regs) : lighting(regs)
		{
			auto alphaTestConfig = regs[InternalRegs::AlphaTestConfig];
			auto alphaTestFunction = Helpers::getBits<4, 3>(alphaTestConfig);

			outConfig.alphaTestFunction = (alphaTestConfig & 1) ? static_cast<PICA::CompareFunction>(alphaTestFunction) : PICA::CompareFunction::Always;
			outConfig.depthMapEnable = regs[InternalRegs::DepthmapEnable] & 1;

			texConfig.texUnitConfig = regs[InternalRegs::TexUnitCfg];
			texConfig.texEnvUpdateBuffer = regs[InternalRegs::TexEnvUpdateBuffer];

			// Set up TEV stages. Annoyingly we can't just memcpy as the TEV registers are arranged like
			// {Source, Operand, Combiner, Color, Scale} and we want to skip the color register since it's uploaded via UBO
		#define setupTevStage(stage)                                                                                    \
			std::memcpy(&texConfig.tevConfigs[stage * 4], &regs[InternalRegs::TexEnv##stage##Source], 3 * sizeof(u32)); \
			texConfig.tevConfigs[stage * 4 + 3] = regs[InternalRegs::TexEnv##stage##Source + 5];

			setupTevStage(0);
			setupTevStage(1);
			setupTevStage(2);
			setupTevStage(3);
			setupTevStage(4);
			setupTevStage(5);
		#undef setupTevStage
		}
	};

	static_assert(
		std::has_unique_object_representations<OutputConfig>() && std::has_unique_object_representations<TextureConfig>() &&
		std::has_unique_object_representations<Light>()
	);
}  // namespace PICA

// Override std::hash for our fragment config class
template <>
struct std::hash<PICA::FragmentConfig> {
	std::size_t operator()(const PICA::FragmentConfig& config) const noexcept { return PICAHash::computeHash((const char*)&config, sizeof(config)); }
};