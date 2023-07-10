#pragma once
#include <array>
#include <span>

#include "PICA/pica_vertex.hpp"
#include "PICA/regs.hpp"
#include "helpers.hpp"

class GPU;

class Renderer {
  protected:
	GPU& gpu;
	static constexpr u32 regNum = 0x300;  // Number of internal PICA registers
	const std::array<u32, regNum>& regs;

  public:
	Renderer(GPU& gpu, const std::array<u32, regNum>& internalRegs);
	virtual ~Renderer();

	static constexpr u32 vertexBufferSize = 0x10000;

	virtual void reset() = 0;
	virtual void display() = 0;                                                              // Display the 3DS screen contents to the window
	virtual void initGraphicsContext() = 0;                                                  // Initialize graphics context
	virtual void clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) = 0;  // Clear a GPU buffer in VRAM
	virtual void displayTransfer(u32 inputAddr, u32 outputAddr, u32 inputSize, u32 outputSize, u32 flags) = 0;  // Perform display transfer
	virtual void drawVertices(PICA::PrimType primType, std::span<const PICA::Vertex> vertices) = 0;             // Draw the given vertices

	virtual void screenshot(const std::string& name) = 0;

	virtual void setFBSize(u32 width, u32 height) = 0;

	virtual void setColourFormat(PICA::ColorFmt format) = 0;
	virtual void setDepthFormat(PICA::DepthFmt format) = 0;

	virtual void setColourBufferLoc(u32 loc) = 0;
	virtual void setDepthBufferLoc(u32 loc) = 0;
};