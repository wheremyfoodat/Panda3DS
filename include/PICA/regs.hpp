#pragma once

namespace PICAInternalRegs {
	enum : u32 {
		// Draw command regs
		VertexCountReg = 0x228,
		VertexOffsetReg = 0x22A,
		SignalDrawArrays = 0x22E,

		// Vertex shader registers
		VertexShaderTransferEnd = 0x2BF,
		VertexShaderTransferIndex = 0x2CB,
		VertexShaderData0 = 0x2CC,
		VertexShaderData1 = 0x2CD,
		VertexShaderData2 = 0x2CE,
		VertexShaderData3 = 0x2CF,
		VertexShaderData4 = 0x2D0,
		VertexShaderData5 = 0x2D1,
		VertexShaderData6 = 0x2D2,
		VertexShaderData7 = 0x2D3,
	};
}