#pragma once
#include "helpers.hpp"

namespace PICA {
	namespace InternalRegs {
		enum : u32 {
			// Rasterizer registers
			ViewportWidth = 0x41,
			ViewportInvw = 0x42,
			ViewportHeight = 0x43,
			ViewportInvh = 0x44,

			// Clipping plane control
			ClipEnable = 0x47,
			ClipData0 = 0x48,
			ClipData1 = 0x49,
			ClipData2 = 0x4A,
			ClipData3 = 0x4B,

			DepthScale = 0x4D,
			DepthOffset = 0x4E,
			ShaderOutputCount = 0x4F,
			ShaderOutmap0 = 0x50,

			ViewportXY = 0x68,
			DepthmapEnable = 0x6D,

			// Texture registers
			TexUnitCfg = 0x80,
			Tex0BorderColor = 0x81,
			Tex1BorderColor = 0x91,
			Tex2BorderColor = 0x99,
			TexEnvUpdateBuffer = 0xE0,
			TexEnvBufferColor = 0xFD,

			// clang-format off
			#define defineTexEnv(index, offset)   \
			TexEnv##index##Source   = offset + 0, \
			TexEnv##index##Operand  = offset + 1, \
			TexEnv##index##Combiner = offset + 2, \
			TexEnv##index##Color    = offset + 3, \
			TexEnv##index##Scale    = offset + 4,

			defineTexEnv(0, 0xC0)
			defineTexEnv(1, 0xC8)
			defineTexEnv(2, 0xD0)
			defineTexEnv(3, 0xD8)
			defineTexEnv(4, 0xF0)
			defineTexEnv(5, 0xF8)

			#undef defineTexEnv
			// clang-format on

			// Fog registers
			FogColor = 0xE1,
			FogLUTIndex = 0xE6,
			FogLUTData0 = 0xE8,
			FogLUTData1 = 0xE9,
			FogLUTData2 = 0xEA,
			FogLUTData3 = 0xEB,
			FogLUTData4 = 0xEC,
			FogLUTData5 = 0xED,
			FogLUTData6 = 0xEE,
			FogLUTData7 = 0xEF,

			// Framebuffer registers
			ColourOperation = 0x100,
			BlendFunc = 0x101,
			LogicOp = 0x102,
			BlendColour = 0x103,
			AlphaTestConfig = 0x104,
			StencilTest = 0x105,
			StencilOp = 0x106,
			DepthAndColorMask = 0x107,
			DepthBufferWrite = 0x115,
			DepthBufferFormat = 0x116,
			ColourBufferFormat = 0x117,
			DepthBufferLoc = 0x11C,
			ColourBufferLoc = 0x11D,
			FramebufferSize = 0x11E,

			// Lighting registers
			LightingEnable = 0x8F,
			Light0Specular0 = 0x140,
			Light0Specular1 = 0x141,
			Light0Diffuse = 0x142,
			Light0Ambient = 0x143,
			Light0XY = 0x144,
			Light0Z = 0x145,
			Light0SpotlightXY = 0x146,
			Light0SpotlightZ = 0x147,
			Light0Config = 0x149,
			Light0AttenuationBias = 0x14A,
			Light0AttenuationScale = 0x14B,

			LightGlobalAmbient = 0x1C0,
			LightNumber = 0x1C2,
			LightConfig0 = 0x1C3,
			LightConfig1 = 0x1C4,
			LightPermutation = 0x1D9,
			LightLUTAbs = 0x1D0,
			LightLUTSelect = 0x1D1,
			LightLUTScale = 0x1D2,

			LightingLUTIndex =  0x01C5,
			LightingLUTData0 =  0x01C8,
			LightingLUTData1 =  0x01C9,
			LightingLUTData2 =  0x01CA,
			LightingLUTData3 =  0x01CB,
			LightingLUTData4 =  0x01CC,
			LightingLUTData5 =  0x01CD,
			LightingLUTData6 =  0x01CE,
			LightingLUTData7 =  0x01CF,
			
			// Geometry pipeline registers
			VertexAttribLoc = 0x200,
			AttribFormatLow = 0x201,
			AttribFormatHigh = 0x202,
			IndexBufferConfig = 0x227,
			VertexCountReg = 0x228,
			VertexOffsetReg = 0x22A,
			SignalDrawArrays = 0x22E,
			SignalDrawElements = 0x22F,

			Attrib0Offset = 0x203,
			Attrib1Offset = 0x206,
			Attrib2Offset = 0x209,
			Attrib3Offset = 0x20C,
			Attrib4Offset = 0x20F,
			Attrib5Offset = 0x212,
			Attrib6Offset = 0x215,
			Attrib7Offset = 0x218,
			Attrib8Offset = 0x21B,
			Attrib9Offset = 0x21E,
			Attrib10Offset = 0x221,
			Attrib11Offset = 0x224,

			Attrib0Config2 = 0x205,
			Attrib1Config2 = 0x208,
			Attrib2Config2 = 0x20B,
			Attrib3Config2 = 0x20E,
			Attrib4Config2 = 0x211,
			Attrib5Config2 = 0x214,
			Attrib6Config2 = 0x217,
			Attrib7Config2 = 0x21A,
			Attrib8Config2 = 0x21D,
			Attrib9Config2 = 0x220,
			Attrib10Config2 = 0x223,
			Attrib11Config2 = 0x226,

			AttribInfoStart = Attrib0Offset,
			AttribInfoEnd = Attrib11Config2,

			// Fixed attribute registers
			FixedAttribIndex = 0x232,
			FixedAttribData0 = 0x233,
			FixedAttribData1 = 0x234,
			FixedAttribData2 = 0x235,

			// Command processor registers
			CmdBufSize0 = 0x238,
			CmdBufSize1 = 0x239,
			CmdBufAddr0 = 0x23A,
			CmdBufAddr1 = 0x23B,
			CmdBufTrigger0 = 0x23C,
			CmdBufTrigger1 = 0x23D,

			PrimitiveConfig = 0x25E,
			PrimitiveRestart = 0x25F,

			// Vertex shader registers
			VertexShaderAttrNum = 0x242,
			VertexBoolUniform = 0x2B0,
			VertexIntUniform0 = 0x2B1,
			VertexIntUniform1 = 0x2B2,
			VertexIntUniform2 = 0x2B3,
			VertexIntUniform3 = 0x2B4,

			VertexShaderEntrypoint = 0x2BA,
			VertexShaderOutputMask = 0x2BD,
			VertexShaderTransferEnd = 0x2BF,
			VertexFloatUniformIndex = 0x2C0,
			VertexFloatUniformData0 = 0x2C1,
			VertexFloatUniformData1 = 0x2C2,
			VertexFloatUniformData2 = 0x2C3,
			VertexFloatUniformData3 = 0x2C4,
			VertexFloatUniformData4 = 0x2C5,
			VertexFloatUniformData5 = 0x2C6,
			VertexFloatUniformData6 = 0x2C7,
			VertexFloatUniformData7 = 0x2C8,

			VertexShaderInputBufferCfg = 0x2B9,
			VertexShaderInputCfgLow = 0x2BB,
			VertexShaderInputCfgHigh = 0x2BC,

			VertexShaderTransferIndex = 0x2CB,
			VertexShaderData0 = 0x2CC,
			VertexShaderData1 = 0x2CD,
			VertexShaderData2 = 0x2CE,
			VertexShaderData3 = 0x2CF,
			VertexShaderData4 = 0x2D0,
			VertexShaderData5 = 0x2D1,
			VertexShaderData6 = 0x2D2,
			VertexShaderData7 = 0x2D3,
			VertexShaderOpDescriptorIndex = 0x2D5,
			VertexShaderOpDescriptorData0 = 0x2D6,
			VertexShaderOpDescriptorData1 = 0x2D7,
			VertexShaderOpDescriptorData2 = 0x2D8,
			VertexShaderOpDescriptorData3 = 0x2D9,
			VertexShaderOpDescriptorData4 = 0x2DA,
			VertexShaderOpDescriptorData5 = 0x2DB,
			VertexShaderOpDescriptorData6 = 0x2DC,
			VertexShaderOpDescriptorData7 = 0x2DD,
		};
	}

	namespace ExternalRegs {
		enum : u32 {
			MemFill1BufferStartPaddr = 0x3,
			MemFill1BufferEndPAddr = 0x4,
			MemFill1Value = 0x5,
			MemFill1Control = 0x6,
			MemFill2BufferStartPaddr = 0x7,
			MemFill2BufferEndPAddr = 0x8,
			MemFill2Value = 0x9,
			MemFill2Control = 0xA,
			VramBankControl = 0xB,
			GPUBusy = 0xC,
			BacklightControl = 0xBC,
			Framebuffer0Size = 0x118,
			Framebuffer0AFirstAddr = 0x119,
			Framebuffer0ASecondAddr = 0x11A,
			Framebuffer0Config = 0x11B,
			Framebuffer0Select = 0x11D,
			Framebuffer0Stride = 0x123,
			Framebuffer0BFirstAddr = 0x124,
			Framebuffer0BSecondAddr = 0x125,
			Framebuffer1Size = 0x156,
			Framebuffer1AFirstAddr = 0x159,
			Framebuffer1ASecondAddr = 0x15A,
			Framebuffer1Config = 0x15B,
			Framebuffer1Select = 0x15D,
			Framebuffer1Stride = 0x163,
			Framebuffer1BFirstAddr = 0x164,
			Framebuffer1BSecondAddr = 0x165,
			TransferInputPAddr = 0x2FF,
			TransferOutputPAddr = 0x300,
			DisplayTransferOutputDim = 0x301,
			DisplayTransferInputDim = 0x302,
			TransferFlags = 0x303,
			TransferTrigger = 0x305,
			TextureCopyTotalBytes = 0x307,
			TextureCopyInputLineGap = 0x308,
			TextureCopyOutputLineGap = 0x309,
		};
	}

	enum class Scaling : u32 {
		None = 0,
		X = 1,
		XY = 2,
	};

	namespace Lights {
		enum : u32 {
			LUT_D0 = 0,
			LUT_D1,
			// LUT 2 is not used, the emulator internally uses it for referring to the current source's spotlight in shaders
			LUT_FR = 0x3,
			LUT_RB,
			LUT_RG,
			LUT_RR,
			LUT_SP0 = 0x8,
			LUT_SP1,
			LUT_SP2,
			LUT_SP3,
			LUT_SP4,
			LUT_SP5,
			LUT_SP6,
			LUT_SP7,
			LUT_DA0 = 0x10,
			LUT_DA1,
			LUT_DA2,
			LUT_DA3,
			LUT_DA4,
			LUT_DA5,
			LUT_DA6,
			LUT_DA7,
			LUT_Count
		};
	}

	// There's actually 8 different LUTs (SP0-SP7), one for each light with different indices (8-15)
	// We use an unused LUT value for "this light source's spotlight" instead and figure out which light source to use in compileLutLookup
	// This is particularly intuitive in several places, such as checking if a LUT is enabled
	static constexpr int spotlightLutIndex = 2;

	enum class TextureFmt : u32 {
		RGBA8 = 0x0,
		RGB8 = 0x1,
		RGBA5551 = 0x2,
		RGB565 = 0x3,
		RGBA4 = 0x4,
		IA8 = 0x5,
		RG8 = 0x6,
		I8 = 0x7,
		A8 = 0x8,
		IA4 = 0x9,
		I4 = 0xA,
		A4 = 0xB,
		ETC1 = 0xC,
		ETC1A4 = 0xD,
	};

	enum class ColorFmt : u32 {
		RGBA8 = 0x0,
		RGB8 = 0x1,
		RGBA5551 = 0x2,
		RGB565 = 0x3,
		RGBA4 = 0x4,
	};

	enum class DepthFmt : u32 {
		Depth16 = 0,
		Unknown1 = 1,  // Technically selectable, but function is unknown
		Depth24 = 2,
		Depth24Stencil8 = 3,
	};

	// Returns the string representation of a texture format
	inline constexpr const char* textureFormatToString(TextureFmt fmt) {
		switch (fmt) {
			case TextureFmt::RGBA8: return "RGBA8";
			case TextureFmt::RGB8: return "RGB8";
			case TextureFmt::RGBA5551: return "RGBA5551";
			case TextureFmt::RGB565: return "RGB565";
			case TextureFmt::RGBA4: return "RGBA4";
			case TextureFmt::IA8: return "IA8";
			case TextureFmt::RG8: return "RG8";
			case TextureFmt::I8: return "I8";
			case TextureFmt::A8: return "A8";
			case TextureFmt::IA4: return "IA4";
			case TextureFmt::I4: return "I4";
			case TextureFmt::A4: return "A4";
			case TextureFmt::ETC1: return "ETC1";
			case TextureFmt::ETC1A4: return "ETC1A4";
			default: return "Unknown";
		}
	}

	inline constexpr const char* textureFormatToString(ColorFmt fmt) {
		return textureFormatToString(static_cast<TextureFmt>(fmt));
	}

	inline constexpr bool hasStencil(DepthFmt format) { return format == PICA::DepthFmt::Depth24Stencil8; }

	// Size occupied by each pixel in bytes

	// All formats are 16BPP except for RGBA8 (32BPP) and BGR8 (24BPP)
	inline constexpr usize sizePerPixel(TextureFmt format) {
		switch (format) {
			case TextureFmt::RGB8: return 3;
			case TextureFmt::RGBA8: return 4;
			default: return 2;
		}
	}

	inline constexpr usize sizePerPixel(ColorFmt format) {
		return sizePerPixel(static_cast<TextureFmt>(format));
	}

	inline constexpr usize sizePerPixel(DepthFmt format) {
		switch (format) {
			case DepthFmt::Depth16: return 2;
			case DepthFmt::Depth24: return 3;
			case DepthFmt::Depth24Stencil8: return 4;
			default: return 1;  // Invalid format
		}
	}

	enum class PrimType : u32 {
		TriangleList = 0,
		TriangleStrip = 1,
		TriangleFan = 2,
		GeometryPrimitive = 3,
	};

	enum class CompareFunction : u32 {
		Never = 0,
		Always = 1,
		Equal = 2,
		NotEqual = 3,
		Less = 4,
		LessOrEqual = 5,
		Greater = 6,
		GreaterOrEqual = 7,
	};

	enum class LogicOpMode : u32 {
		Clear = 0,
		And = 1,
		ReverseAnd = 2,
		Copy = 3,
		Set = 4,
		InvertedCopy = 5,
		Nop = 6,
		Invert = 7,
		Nand = 8,
		Or = 9,
		Nor = 10,
		Xor = 11,
		Equiv = 12,
		InvertedAnd = 13,
		ReverseOr = 14,
		InvertedOr = 15,
	};

	enum class FogMode : u32 {
		Disabled = 0,
		Fog = 5,
		Gas = 7,
	};

	struct TexEnvConfig {
		enum class Source : u8 {
			PrimaryColor = 0x0,
			PrimaryFragmentColor = 0x1,
			SecondaryFragmentColor = 0x2,
			Texture0 = 0x3,
			Texture1 = 0x4,
			Texture2 = 0x5,
			Texture3 = 0x6,
			// TODO: Inbetween values are unknown
			PreviousBuffer = 0xD,
			Constant = 0xE,
			Previous = 0xF,
		};

		enum class ColorOperand : u8 {
			SourceColor = 0x0,
			OneMinusSourceColor = 0x1,
			SourceAlpha = 0x2,
			OneMinusSourceAlpha = 0x3,
			SourceRed = 0x4,
			OneMinusSourceRed = 0x5,
			// TODO: Inbetween values are unknown
			SourceGreen = 0x8,
			OneMinusSourceGreen = 0x9,
			// Inbetween values are unknown
			SourceBlue = 0xC,
			OneMinusSourceBlue = 0xD,
		};

		enum class AlphaOperand : u8 {
			SourceAlpha = 0x0,
			OneMinusSourceAlpha = 0x1,
			SourceRed = 0x2,
			OneMinusSourceRed = 0x3,
			SourceGreen = 0x4,
			OneMinusSourceGreen = 0x5,
			SourceBlue = 0x6,
			OneMinusSourceBlue = 0x7,
		};

		enum class Operation : u8 {
			Replace = 0,
			Modulate = 1,
			Add = 2,
			AddSigned = 3,
			Lerp = 4,
			Subtract = 5,
			Dot3RGB = 6,
			Dot3RGBA = 7,
			MultiplyAdd = 8,
			AddMultiply = 9,
		};

		// RGB sources
		Source colorSource1, colorSource2, colorSource3;
		// Alpha sources
		Source alphaSource1, alphaSource2, alphaSource3;

		// RGB operands
		ColorOperand colorOperand1, colorOperand2, colorOperand3;
		// Alpha operands
		AlphaOperand alphaOperand1, alphaOperand2, alphaOperand3;

		// Texture environment operations for this stage
		Operation colorOp, alphaOp;

		u32 constColor;

	  private:
		// These are the only private members since their value doesn't actually reflect the scale
		// So we make them public so we'll always use the appropriate member functions instead
		u8 colorScale;
		u8 alphaScale;

	  public:
		// Create texture environment object from TEV registers
		TexEnvConfig(u32 source, u32 operand, u32 combiner, u32 color, u32 scale) : constColor(color) {
			colorSource1 = Helpers::getBits<0, 4, Source>(source);
			colorSource2 = Helpers::getBits<4, 4, Source>(source);
			colorSource3 = Helpers::getBits<8, 4, Source>(source);

			alphaSource1 = Helpers::getBits<16, 4, Source>(source);
			alphaSource2 = Helpers::getBits<20, 4, Source>(source);
			alphaSource3 = Helpers::getBits<24, 4, Source>(source);

			colorOperand1 = Helpers::getBits<0, 4, ColorOperand>(operand);
			colorOperand2 = Helpers::getBits<4, 4, ColorOperand>(operand);
			colorOperand3 = Helpers::getBits<8, 4, ColorOperand>(operand);

			alphaOperand1 = Helpers::getBits<12, 3, AlphaOperand>(operand);
			alphaOperand2 = Helpers::getBits<16, 3, AlphaOperand>(operand);
			alphaOperand3 = Helpers::getBits<20, 3, AlphaOperand>(operand);

			colorOp = Helpers::getBits<0, 4, Operation>(combiner);
			alphaOp = Helpers::getBits<16, 4, Operation>(combiner);

			colorScale = Helpers::getBits<0, 2>(scale);
			alphaScale = Helpers::getBits<16, 2>(scale);
		}

		u32 getColorScale() { return (colorScale <= 2) ? (1 << colorScale) : 1; }
		u32 getAlphaScale() { return (alphaScale <= 2) ? (1 << alphaScale) : 1; }

		bool isPassthroughStage() {
			// clang-format off
			// Thank you to the Citra dev that wrote this out
			return (
				colorOp == Operation::Replace && alphaOp == Operation::Replace &&
				colorSource1 == Source::Previous && alphaSource1 == Source::Previous &&
				colorOperand1 == ColorOperand::SourceColor && alphaOperand1 == AlphaOperand::SourceAlpha &&
				getColorScale() == 1 && getAlphaScale() == 1
			);
			// clang-format on
		}
	};
}  // namespace PICA
