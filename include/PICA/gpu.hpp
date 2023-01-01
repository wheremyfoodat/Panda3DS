#pragma once
#include <array>
#include "helpers.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "PICA/float_types.hpp"
#include "PICA/shader_unit.hpp"
#include "renderer_gl/renderer_gl.hpp"

class GPU {
	using vec4f = OpenGL::Vector<Floats::f24, 4>;

	Memory& mem;
	ShaderUnit shaderUnit;
	u8* vram = nullptr;
	MAKE_LOG_FUNCTION(log, gpuLogger)

	static constexpr u32 maxAttribCount = 12; // Up to 12 vertex attributes
	static constexpr u32 regNum = 0x300;
	static constexpr u32 vramSize = 6_MB;
	std::array<u32, regNum> regs; // GPU internal registers

	// Read a value of type T from physical address paddr
	// This is necessary because vertex attribute fetching uses physical addresses
	template<typename T>
	T readPhysical(u32 paddr) {
		if (paddr >= PhysicalAddrs::FCRAM && paddr <= PhysicalAddrs::FCRAMEnd) {
			u8* fcram = mem.getFCRAM();
			u32 index = paddr - PhysicalAddrs::FCRAM;

			return *(T*)&fcram[index];
		} else {
			Helpers::panic("[PICA] Read unimplemented paddr %08X", paddr);
		}
	}

	// Get a pointer of type T* to the data starting from physical address paddr
	template<typename T>
	T* getPointerPhys(u32 paddr) {
		if (paddr >= PhysicalAddrs::FCRAM && paddr <= PhysicalAddrs::FCRAMEnd) {
			u8* fcram = mem.getFCRAM();
			u32 index = paddr - PhysicalAddrs::FCRAM;

			return (T*)&fcram[index];
		} else if (paddr >= PhysicalAddrs::VRAM && paddr <= PhysicalAddrs::VRAMEnd) {
			u32 index = paddr - PhysicalAddrs::VRAM;
			return (T*)&vram[index];
		} else [[unlikely]] {
			Helpers::panic("[GPU] Tried to access unknown physical address: %08X", paddr);
		}
	}

	template <bool indexed>
	void drawArrays();

	// Silly method of avoiding linking problems. TODO: Change to something less silly
	void drawArrays(bool indexed);

	struct AttribInfo {
		u32 offset = 0; // Offset from base vertex array
		int size = 0; // Bytes per vertex
		u32 config1 = 0;
		u32 config2 = 0;

		u64 getConfigFull() {
			return u64(config1) | (u64(config2) << 32);
		}
	};

	std::array<AttribInfo, maxAttribCount> attributeInfo; // Info for each of the 12 attributes
	u32 totalAttribCount = 0; // Number of vertex attributes to send to VS
	u32 fixedAttribMask = 0; // Which attributes are fixed?
	
	u32 fixedAttribIndex = 0; // Which fixed attribute are we writing to ([0, 11] range)
	u32 fixedAttribCount = 0; // How many attribute components have we written? When we get to 4 the attr will actually get submitted
	std::array<u32, 3> fixedAttrBuff; // Buffer to hold fixed attributes in until they get submitted

	Renderer renderer;

public:
	GPU(Memory& mem);
	void initGraphicsContext() { renderer.initGraphicsContext(); }
	void getGraphicsContext() { renderer.getGraphicsContext(); }
	void display() { renderer.display(); }

	void fireDMA(u32 dest, u32 source, u32 size);
	void reset();

	// Used by the GSP GPU service for readHwRegs/writeHwRegs/writeHwRegsMasked
	u32 readReg(u32 address);
	void writeReg(u32 address, u32 value);

	// Used when processing GPU command lists
	u32 readInternalReg(u32 index);
	void writeInternalReg(u32 index, u32 value, u32 mask);

	void clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) {
		renderer.clearBuffer(startAddress, endAddress, value, control);
	}
};