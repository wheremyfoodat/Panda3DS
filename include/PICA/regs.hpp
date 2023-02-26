#pragma once

namespace PICAInternalRegs {
	enum : u32 {
		// Rasterizer registers
		ViewportWidth = 0x41,
		ViewportInvw = 0x42,
		ViewportHeight = 0x43,
		ViewportInvh = 0x44,

		DepthScale = 0x4D,
		DepthOffset = 0x4E,
		ShaderOutputCount = 0x4F,

		// Framebuffer registers
		ColourOperation = 0x100,
		BlendFunc = 0x101,
		BlendColour = 0x103,
		AlphaTestConfig = 0x104,
		DepthAndColorMask = 0x107,
		ColourBufferFormat = 0x117,
		DepthBufferLoc = 0x11C,
		ColourBufferLoc = 0x11D,
		FramebufferSize = 0x11E,

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

		PrimitiveConfig = 0x25E,

		// Vertex shader registers
		VertexBoolUniform = 0x2B0,
		VertexIntUniform0 = 0x2B1,
		VertexIntUniform1 = 0x2B2,
		VertexIntUniform2 = 0x2B3,
		VertexIntUniform3 = 0x2B4,

		VertexShaderEntrypoint = 0x2BA,
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