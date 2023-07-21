#pragma once
#include <array>
#include <span>
#include <optional>

#include "PICA/pica_vertex.hpp"
#include "PICA/regs.hpp"
#include "helpers.hpp"

enum class RendererType : s8 {
	// Todo: Auto = -1,
	Null = 0,
	OpenGL = 1,
	Vulkan = 2,
};

class GPU;

class Renderer {
  protected:
	GPU& gpu;
	static constexpr u32 regNum = 0x300;  // Number of internal PICA registers
	const std::array<u32, regNum>& regs;

	std::array<u32, 2> fbSize;  // The size of the framebuffer (ie both the colour and depth buffer)'

	u32 colourBufferLoc;                // Location in 3DS VRAM for the colour buffer
	PICA::ColorFmt colourBufferFormat;  // Format of the colours stored in the colour buffer

	// Same for the depth/stencil buffer
	u32 depthBufferLoc;
	PICA::DepthFmt depthBufferFormat;

  public:
	Renderer(GPU& gpu, const std::array<u32, regNum>& internalRegs);
	virtual ~Renderer();

	static constexpr u32 vertexBufferSize = 0x10000;
	static std::optional<RendererType> typeFromString(std::string inString);
	static const char* typeToString(RendererType rendererType);

	virtual void reset() = 0;
	virtual void display() = 0;                                                              // Display the 3DS screen contents to the window
	virtual void initGraphicsContext() = 0;                                                  // Initialize graphics context
	virtual void clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) = 0;  // Clear a GPU buffer in VRAM
	virtual void displayTransfer(u32 inputAddr, u32 outputAddr, u32 inputSize, u32 outputSize, u32 flags) = 0;  // Perform display transfer
	virtual void drawVertices(PICA::PrimType primType, std::span<const PICA::Vertex> vertices) = 0;             // Draw the given vertices

	virtual void screenshot(const std::string& name) = 0;

	void setFBSize(u32 width, u32 height) {
		fbSize[0] = width;
		fbSize[1] = height;
	}

	void setColourFormat(PICA::ColorFmt format) { colourBufferFormat = format; }
	void setDepthFormat(PICA::DepthFmt format) {
		if (format == PICA::DepthFmt::Unknown1) {
			Helpers::panic("[PICA] Undocumented depth-stencil mode!");
		}
		depthBufferFormat = format;
	}

	void setColourBufferLoc(u32 loc) { colourBufferLoc = loc; }
	void setDepthBufferLoc(u32 loc) { depthBufferLoc = loc; }
};