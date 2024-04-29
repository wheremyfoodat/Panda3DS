#pragma once
#include <array>

#include "PICA/dynapica/shader_rec.hpp"
#include "PICA/float_types.hpp"
#include "PICA/pica_vertex.hpp"
#include "PICA/regs.hpp"
#include "PICA/shader_unit.hpp"
#include "compiler_builtins.hpp"
#include "config.hpp"
#include "helpers.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "renderer.hpp"

class GPU {
	static constexpr u32 regNum = 0x300;
	static constexpr u32 extRegNum = 0x1000;

	using vec4f = std::array<Floats::f24, 4>;
	using Registers = std::array<u32, regNum>;  // Internal registers (named registers in short since they're the main ones)
	using ExternalRegisters = std::array<u32, extRegNum>;

	Memory& mem;
	EmulatorConfig& config;
	ShaderUnit shaderUnit;
	ShaderJIT shaderJIT;  // Doesn't do anything if JIT is disabled or not supported

	u8* vram = nullptr;
	MAKE_LOG_FUNCTION(log, gpuLogger)

	static constexpr u32 maxAttribCount = 12;  // Up to 12 vertex attributes
	static constexpr u32 vramSize = u32(6_MB);
	Registers regs;                           // GPU internal registers
	std::array<vec4f, 16> currentAttributes;  // Vertex attributes before being passed to the shader

	std::array<vec4f, 16> immediateModeAttributes;  // Vertex attributes uploaded via immediate mode submission
	std::array<PICA::Vertex, 3> immediateModeVertices;

	// Pointers for the output registers as arranged after GPUREG_VSH_OUTMAP_MASK is applied
	std::array<Floats::f24*, 16> vsOutputRegisters;
	// Previous value for GPUREG_VSH_OUTMAP_MASK
	u32 oldVsOutputMask;

	uint immediateModeVertIndex;
	uint immediateModeAttrIndex;  // Index of the immediate mode attribute we're uploading

	template <bool indexed, bool useShaderJIT>
	void drawArrays();

	// Silly method of avoiding linking problems. TODO: Change to something less silly
	void drawArrays(bool indexed);

	struct AttribInfo {
		u32 offset = 0;  // Offset from base vertex array
		int size = 0;    // Bytes per vertex
		u32 config1 = 0;
		u32 config2 = 0;
		u32 componentCount = 0;  // Number of components for the attribute

		u64 getConfigFull() { return u64(config1) | (u64(config2) << 32); }
	};

	u64 getVertexShaderInputConfig() {
		return u64(regs[PICA::InternalRegs::VertexShaderInputCfgLow]) | (u64(regs[PICA::InternalRegs::VertexShaderInputCfgHigh]) << 32);
	}

	std::array<AttribInfo, maxAttribCount> attributeInfo;  // Info for each of the 12 attributes
	u32 totalAttribCount = 0;                              // Number of vertex attributes to send to VS
	u32 fixedAttribMask = 0;                               // Which attributes are fixed?

	u32 fixedAttribIndex = 0;          // Which fixed attribute are we writing to ([0, 11] range)
	u32 fixedAttribCount = 0;          // How many attribute components have we written? When we get to 4 the attr will actually get submitted
	std::array<u32, 3> fixedAttrBuff;  // Buffer to hold fixed attributes in until they get submitted

	// Command processor pointers for GPU command lists
	u32* cmdBuffStart = nullptr;
	u32* cmdBuffEnd = nullptr;
	u32* cmdBuffCurr = nullptr;

	std::unique_ptr<Renderer> renderer;
	PICA::Vertex getImmediateModeVertex();

  public:
	// 256 entries per LUT with each LUT as its own row forming a 2D image 256 * LUT_COUNT
	// Encoded in PICA native format
	static constexpr size_t LightingLutSize = PICA::Lights::LUT_Count * 256;
	std::array<uint32_t, LightingLutSize> lightingLUT;

	// Used to prevent uploading the lighting_lut on every draw call
	// Set to true when the CPU writes to the lighting_lut
	// Set to false by the renderer when the lighting_lut is uploaded ot the GPU
	bool lightingLUTDirty = false;

	GPU(Memory& mem, EmulatorConfig& config);
	void display() { renderer->display(); }
	void screenshot(const std::string& name) { renderer->screenshot(name); }
	void deinitGraphicsContext() { renderer->deinitGraphicsContext(); }

#if defined(PANDA3DS_FRONTEND_SDL)
	void initGraphicsContext(SDL_Window* window) { renderer->initGraphicsContext(window); }
#elif defined(PANDA3DS_FRONTEND_QT)
	void initGraphicsContext(GL::Context* context) { renderer->initGraphicsContext(context); }
#endif

	void fireDMA(u32 dest, u32 source, u32 size);
	void reset();

	Registers& getRegisters() { return regs; }
	ExternalRegisters& getExtRegisters() { return externalRegs; }
	void startCommandList(u32 addr, u32 size);

	// Used by the GSP GPU service for readHwRegs/writeHwRegs/writeHwRegsMasked
	u32 readReg(u32 address);
	void writeReg(u32 address, u32 value);

	u32 readExternalReg(u32 index);
	void writeExternalReg(u32 index, u32 value);

	// Used when processing GPU command lists
	u32 readInternalReg(u32 index);
	void writeInternalReg(u32 index, u32 value, u32 mask);

	// Used for setting the size of the window we'll be outputting graphics to
	void setOutputSize(u32 width, u32 height) { renderer->setOutputSize(width, height); }

	// TODO: Emulate the transfer engine & its registers
	// Then this can be emulated by just writing the appropriate values there
	void clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) { renderer->clearBuffer(startAddress, endAddress, value, control); }

	// TODO: Emulate the transfer engine & its registers
	// Then this can be emulated by just writing the appropriate values there
	void displayTransfer(u32 inputAddr, u32 outputAddr, u32 inputSize, u32 outputSize, u32 flags) {
		renderer->displayTransfer(inputAddr, outputAddr, inputSize, outputSize, flags);
	}

	void textureCopy(u32 inputAddr, u32 outputAddr, u32 totalBytes, u32 inputSize, u32 outputSize, u32 flags) {
		renderer->textureCopy(inputAddr, outputAddr, totalBytes, inputSize, outputSize, flags);
	}

	// Read a value of type T from physical address paddr
	// This is necessary because vertex attribute fetching uses physical addresses
	template <typename T>
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
	template <typename T>
	T* getPointerPhys(u32 paddr, u32 size = 0) {
		if (paddr >= PhysicalAddrs::FCRAM && paddr + size <= PhysicalAddrs::FCRAMEnd) {
			u8* fcram = mem.getFCRAM();
			u32 index = paddr - PhysicalAddrs::FCRAM;

			return (T*)&fcram[index];
		} else if (paddr >= PhysicalAddrs::VRAM && paddr + size <= PhysicalAddrs::VRAMEnd) {
			u32 index = paddr - PhysicalAddrs::VRAM;
			return (T*)&vram[index];
		} else [[unlikely]] {
			Helpers::panic("[GPU] Tried to access unknown physical address: %08X", paddr);
		}
	}

	Renderer* getRenderer() { return renderer.get(); }
  private:
	// GPU external registers
	// We have them in the end of the struct for cache locality reasons. Tl;dr we want the more commonly used things to be packed in the start
	// Of the struct, instead of externalRegs being in the middle
	ExternalRegisters externalRegs;

	ALWAYS_INLINE void setVsOutputMask(u32 val) {
		val &= 0xffff;
	
		// Avoid recomputing this if not necessary 
		if (oldVsOutputMask != val) [[unlikely]] {
			oldVsOutputMask = val;

			uint count = 0;
			// See which registers are actually enabled and ignore the disabled ones
			for (int i = 0; i < 16; i++) {
				if (val & 1) {
					vsOutputRegisters[count++] = &shaderUnit.vs.outputs[i][0];
				}

				val >>= 1;
			}

			// For the others, map the index to a vs output directly (TODO: What does hw actually do?)
			for (; count < 16; count++) {
				vsOutputRegisters[count] = &shaderUnit.vs.outputs[count][0];
			}
		}
	}
};
