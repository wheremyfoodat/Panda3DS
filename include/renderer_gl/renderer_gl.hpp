#pragma once
#include <array>
#include "helpers.hpp"
#include "logger.hpp"
#include "opengl.hpp"
#include "surface_cache.hpp"
#include "textures.hpp"

// More circular dependencies!
class GPU;

struct Vertex {
	OpenGL::vec4 position;
	OpenGL::vec4 colour;
	OpenGL::vec2 UVs;
};

class Renderer {
	GPU& gpu;
	OpenGL::Program triangleProgram;
	OpenGL::Program displayProgram;

	OpenGL::VertexArray vao;
	OpenGL::VertexBuffer vbo;
	GLint alphaControlLoc = -1;
	GLint texUnitConfigLoc = -1;
	
	// Depth configuration uniform locations
	GLint depthOffsetLoc = -1;
	GLint depthScaleLoc = -1;
	GLint depthmapEnableLoc = -1;

	u32 oldAlphaControl = 0;
	u32 oldTexUnitConfig = 0;

	float oldDepthScale = -1.0;
	float oldDepthOffset = 0.0;
	bool oldDepthmapEnable = false;

	SurfaceCache<DepthBuffer, 10> depthBufferCache;
	SurfaceCache<ColourBuffer, 10> colourBufferCache;
	SurfaceCache<Texture, 16> textureCache;

	OpenGL::uvec2 fbSize; // The size of the framebuffer (ie both the colour and depth buffer)'

	u32 colourBufferLoc; // Location in 3DS VRAM for the colour buffer
	ColourBuffer::Formats colourBufferFormat; // Format of the colours stored in the colour buffer
	
	// Same for the depth/stencil buffer
	u32 depthBufferLoc;
	DepthBuffer::Formats depthBufferFormat;

	// Dummy VAO/VBO for blitting the final output
	OpenGL::VertexArray dummyVAO;
	OpenGL::VertexBuffer dummyVBO;

	static constexpr u32 regNum = 0x300; // Number of internal PICA registers
	const std::array<u32, regNum>& regs;

	OpenGL::Framebuffer getColourFBO();
	OpenGL::Texture getTexture(Texture& tex);

	MAKE_LOG_FUNCTION(log, rendererLogger)
	void setupBlending();
	void bindDepthBuffer();

public:
	Renderer(GPU& gpu, const std::array<u32, regNum>& internalRegs) : gpu(gpu), regs(internalRegs) {}

	void reset();
	void display(); // Display the 3DS screen contents to the window
	void initGraphicsContext(); // Initialize graphics context
	void getGraphicsContext();  // Set up graphics context for rendering
	void clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control); // Clear a GPU buffer in VRAM
	void displayTransfer(u32 inputAddr, u32 outputAddr, u32 inputSize, u32 outputSize, u32 flags); // Perform display transfer
	void drawVertices(OpenGL::Primitives primType, Vertex* vertices, u32 count); // Draw the given vertices

	void setFBSize(u32 width, u32 height) {
		fbSize.x() = width;
		fbSize.y() = height;
	}

	void setColourFormat(ColourBuffer::Formats format) { colourBufferFormat = format; }
	void setColourFormat(u32 format) { colourBufferFormat = static_cast<ColourBuffer::Formats>(format); }

	void setDepthFormat(DepthBuffer::Formats format) { depthBufferFormat = format; }
	void setDepthFormat(u32 format) {
		if (format == 2) {
			Helpers::panic("[PICA] Undocumented depth-stencil mode!");
		}

		depthBufferFormat = static_cast<DepthBuffer::Formats>(format);
	}

	void setColourBufferLoc(u32 loc) { colourBufferLoc = loc; }
	void setDepthBufferLoc(u32 loc) { depthBufferLoc = loc; }

	static constexpr u32 vertexBufferSize = 0x1500;
};