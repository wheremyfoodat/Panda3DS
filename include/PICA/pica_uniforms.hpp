#pragma once

#include <array>
#include <span>
#include "helpers.hpp"
#include "math_util.hpp"

struct alignas(64) VsSupportBuffer {
	explicit VsSupportBuffer(std::span<const u32> regs);

	Math::vec4 clipData;
};
static_assert(sizeof(VsSupportBuffer) == 64);

struct Light {
	alignas(16) Math::vec3 specular_0;
	alignas(16) Math::vec3 specular_1;
	alignas(16) Math::vec3 diffuse;
	alignas(16) Math::vec3 ambient;
	alignas(16) Math::vec3 position;
	alignas(16) Math::vec3 spot_direction;
	u32 config;
};

struct TexEnv {
	Math::vec4 color;
	u32 source;
	u32 operand;
	u32 combiner;
	u32 scale;
};

struct alignas(64) FsSupportBuffer {
	explicit FsSupportBuffer(std::span<const u32> regs);

	Math::vec4 textureEnvBufferColor;
	alignas(16) std::array<TexEnv, 6> textureEnv;
	u32 textureConfig;
	u32 textureEnvUpdateBuffer;
	u32 alphaControl;
	float depthScale;
	float depthOffset;
	u32 depthMapEnable;
	u32 lightingEnable;
	u32 numLights;
	alignas(16) Math::vec4 lightingAmbient;
	u32 lightPermutation;
	u32 lightingLutInputAbs;
	u32 lightingLutInputSelect;
	u32 lightingLutInputScale;
	u32 lightingConfig0;
	u32 lightingConfig1;
	std::array<Light, 8> lights;
};

static_assert(sizeof(FsSupportBuffer) == 1088);
