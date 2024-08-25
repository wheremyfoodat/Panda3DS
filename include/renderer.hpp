#pragma once
#include <array>
#include <optional>
#include <span>
#include <string>

#include "PICA/draw_acceleration.hpp"
#include "PICA/pica_vertex.hpp"
#include "PICA/regs.hpp"
#include "helpers.hpp"

#ifdef PANDA3DS_FRONTEND_QT
#include "gl/context.h"
#endif

enum class RendererType : s8 {
	// Todo: Auto = -1,
	Null = 0,
	OpenGL = 1,
	Vulkan = 2,
	Software = 3,
};

struct EmulatorConfig;
struct SDL_Window;

class GPU;
class ShaderUnit;

class Renderer {
  protected:
	GPU& gpu;
	static constexpr u32 regNum = 0x300;      // Number of internal PICA registers
	static constexpr u32 extRegNum = 0x1000;  // Number of external PICA registers

	const std::array<u32, regNum>& regs;
	const std::array<u32, extRegNum>& externalRegs;

	std::array<u32, 2> fbSize;  // The size of the framebuffer (ie both the colour and depth buffer)'

	u32 colourBufferLoc;                // Location in 3DS VRAM for the colour buffer
	PICA::ColorFmt colourBufferFormat;  // Format of the colours stored in the colour buffer

	// Same for the depth/stencil buffer
	u32 depthBufferLoc;
	PICA::DepthFmt depthBufferFormat;

	// Width and height of the window we're outputting to, needed for properly scaling the final image
	// We initialize it to the 3DS resolution by default and the frontend can notify us if it changes via the setOutputSize function
	u32 outputWindowWidth = 400;
	u32 outputWindowHeight = 240 * 2;

	EmulatorConfig* emulatorConfig = nullptr;

  public:
	Renderer(GPU& gpu, const std::array<u32, regNum>& internalRegs, const std::array<u32, extRegNum>& externalRegs);
	virtual ~Renderer();

	static constexpr u32 vertexBufferSize = 0x10000;
	static std::optional<RendererType> typeFromString(std::string inString);
	static const char* typeToString(RendererType rendererType);

	virtual void reset() = 0;
	virtual void display() = 0;                                                              // Display the 3DS screen contents to the window
	virtual void initGraphicsContext(SDL_Window* window) = 0;                                // Initialize graphics context
	virtual void clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) = 0;  // Clear a GPU buffer in VRAM
	virtual void displayTransfer(u32 inputAddr, u32 outputAddr, u32 inputSize, u32 outputSize, u32 flags) = 0;  // Perform display transfer
	virtual void textureCopy(u32 inputAddr, u32 outputAddr, u32 totalBytes, u32 inputSize, u32 outputSize, u32 flags) = 0;
	virtual void drawVertices(PICA::PrimType primType, std::span<const PICA::Vertex> vertices) = 0;  // Draw the given vertices

	virtual void screenshot(const std::string& name) = 0;
	// Some frontends and platforms may require that we delete our GL or misc context and obtain a new one for things like exclusive fullscreen
	// This function does things like write back or cache necessary state before we delete our context
	virtual void deinitGraphicsContext() = 0;

	// Functions for hooking up the renderer core to the frontend's shader editor for editing ubershaders in real time
	// SupportsShaderReload: Indicates whether the backend offers ubershader reload support or not
	// GetUbershader/SetUbershader: Gets or sets the renderer's current ubershader
	virtual bool supportsShaderReload() { return false; }
	virtual std::string getUbershader() { return ""; }
	virtual void setUbershader(const std::string& shader) {}

	// This function is called on every draw call before parsing vertex data.
	// It is responsible for things like looking up which vertex/fragment shaders to use, recompiling them if they don't exist, choosing between
	// ubershaders and shadergen, and so on.
	// Returns whether this draw is eligible for using hardware-accelerated shaders or if shaders should run on the CPU
	virtual bool prepareForDraw(ShaderUnit& shaderUnit, PICA::DrawAcceleration* accel) { return false; }

	// Functions for initializing the graphics context for the Qt frontend, where we don't have the convenience of SDL_Window
#ifdef PANDA3DS_FRONTEND_QT
	virtual void initGraphicsContext(GL::Context* context) { Helpers::panic("Tried to initialize incompatible renderer with GL context"); }
#endif

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

	void setOutputSize(u32 width, u32 height) {
		outputWindowWidth = width;
		outputWindowHeight = height;
	}

	void setConfig(EmulatorConfig* config) { emulatorConfig = config; }
};
