#include "PICA/pica_uniforms.hpp"
#include "PICA/float_types.hpp"
#include "PICA/regs.hpp"

using Floats::f16;
using Floats::f24;

Math::vec4 abgr8888ToVec4(uint abgr) {
	const float scale = 1.0 / 255.0;
	return Math::vec4({float(abgr & 0xffu) * scale, float((abgr >> 8) & 0xffu) * scale,
					   float((abgr >> 16) & 0xffu) * scale, float(abgr >> 24) * scale});
}

Math::vec3 regToColor3(uint reg) {
	// Normalization scale to convert from [0...255] to [0.0...1.0]
	const float scale = 1.0 / 255.0;
	return Math::vec3({float(Helpers::getBits<20, 8>(reg)) * scale, float(Helpers::getBits<10, 8>(reg)) * scale,
					   float(Helpers::getBits<0, 8>(reg)) * scale});
}

Math::vec4 regToColor4(uint reg) {
	// Normalization scale to convert from [0...255] to [0.0...1.0]
	const float scale = 1.0 / 255.0;
	return Math::vec4({float(Helpers::getBits<20, 8>(reg)) * scale, float(Helpers::getBits<10, 8>(reg)) * scale,
					   float(Helpers::getBits<0, 8>(reg)) * scale, 1.f});
}

VsSupportBuffer::VsSupportBuffer(std::span<const u32> regs) {
	clipData.x() = f24::fromRaw(regs[PICA::InternalRegs::ClipData0]).toFloat32();
	clipData.y() = f24::fromRaw(regs[PICA::InternalRegs::ClipData1]).toFloat32();
	clipData.z() = f24::fromRaw(regs[PICA::InternalRegs::ClipData2]).toFloat32();
	clipData.w() = f24::fromRaw(regs[PICA::InternalRegs::ClipData3]).toFloat32();
}


FsSupportBuffer::FsSupportBuffer(std::span<const u32> regs) {
	static constexpr std::array<u32, 6> ioBases = {
		PICA::InternalRegs::TexEnv0Source, PICA::InternalRegs::TexEnv1Source, PICA::InternalRegs::TexEnv2Source,
		PICA::InternalRegs::TexEnv3Source, PICA::InternalRegs::TexEnv4Source, PICA::InternalRegs::TexEnv5Source,
	};

	// TEV configuration
	textureEnvBufferColor = abgr8888ToVec4(regs[PICA::InternalRegs::TexEnvBufferColor]);
	textureEnvUpdateBuffer = regs[PICA::InternalRegs::TexEnvUpdateBuffer];
	textureConfig = regs[PICA::InternalRegs::TexUnitCfg];

	for (int i = 0; i < ioBases.size(); i++) {
		const u32 ioBase = ioBases[i];
		textureEnv[i].source = regs[ioBase];
		textureEnv[i].operand = regs[ioBase + 1];
		textureEnv[i].combiner = regs[ioBase + 2];
		textureEnv[i].color = abgr8888ToVec4(regs[ioBase + 3]);
		textureEnv[i].scale = regs[ioBase + 4];
	}

	// Alpha testing
	alphaControl = regs[PICA::InternalRegs::AlphaTestConfig];

	// Depth testing
	depthScale = f24::fromRaw(regs[PICA::InternalRegs::DepthScale] & 0xffffff).toFloat32();
	depthOffset = f24::fromRaw(regs[PICA::InternalRegs::DepthOffset] & 0xffffff).toFloat32();
	depthMapEnable = regs[PICA::InternalRegs::DepthmapEnable] & 1;

	// Lighting
	lightingEnable = regs[PICA::InternalRegs::LightingEnable] & 1;
	lightingAmbient = regToColor4(regs[PICA::InternalRegs::LightingAmbient]);
	numLights = (regs[PICA::InternalRegs::LightingNumLights] & 7) +	1;
	lightPermutation = regs[PICA::InternalRegs::LightingLightPermutation];
	lightingLutInputAbs = regs[PICA::InternalRegs::LightingLutInputAbs];
	lightingLutInputSelect = regs[PICA::InternalRegs::LightingLutInputSelect];
	lightingLutInputScale = regs[PICA::InternalRegs::LightingLutInputScale];
	lightingConfig0 = regs[PICA::InternalRegs::LightingConfig0];
	lightingConfig1 = regs[PICA::InternalRegs::LightingConfig1];

	static constexpr std::array<u32, 8> lightBases = {
		PICA::InternalRegs::Light0Specular0, PICA::InternalRegs::Light1Specular0, PICA::InternalRegs::Light2Specular0,
		PICA::InternalRegs::Light3Specular0, PICA::InternalRegs::Light4Specular0, PICA::InternalRegs::Light5Specular0,
		PICA::InternalRegs::Light6Specular0, PICA::InternalRegs::Light7Specular0
	};

	// Lights
	for (int i = 0; i < lights.size(); i++) {
		const u32 lightBase = lightBases[i];
		lights[i].specular_0 = regToColor3(regs[lightBase]);
		lights[i].specular_1 = regToColor3(regs[lightBase + 1]);
		lights[i].diffuse = regToColor3(regs[lightBase + 2]);
		lights[i].ambient = regToColor3(regs[lightBase + 3]);
		lights[i].position.x() = f16::fromRaw(regs[lightBase + 4] & 0xFFFF).toFloat32();
		lights[i].position.y() = f16::fromRaw(regs[lightBase + 4] >> 16).toFloat32();
		lights[i].position.z() = f16::fromRaw(regs[lightBase + 5] & 0xFFFF).toFloat32();
		lights[i].spot_direction.x() = (regs[lightBase + 6] & 0xFFF) / 2047.0f;
		lights[i].spot_direction.y() = ((regs[lightBase + 6] >> 16) & 0xFFF) / 2047.0f;
		lights[i].spot_direction.z() = (regs[lightBase + 7] & 0xFFF) / 2047.0f;
		lights[i].config = regs[lightBase + 9];
	}
}
