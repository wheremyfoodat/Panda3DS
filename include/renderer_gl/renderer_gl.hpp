#pragma once
#include <stb_image_write.h>

#include <array>
#include <span>

#include "PICA/float_types.hpp"
#include "PICA/pica_vertex.hpp"
#include "PICA/regs.hpp"
#include "gl_state.hpp"
#include "helpers.hpp"
#include "logger.hpp"
#include "renderer.hpp"
#include "surface_cache.hpp"
#include "textures.hpp"

// More circular dependencies!
class GPU;

class RendererGL final : public Renderer {
	GLStateManager gl = {};

	OpenGL::Program triangleProgram;
	OpenGL::Program displayProgram;

	OpenGL::VertexArray vao;
	OpenGL::VertexBuffer vbo;

	// TEV configuration uniform locations
	GLint textureEnvSourceLoc = -1;
	GLint textureEnvOperandLoc = -1;
	GLint textureEnvCombinerLoc = -1;
	GLint textureEnvColorLoc = -1;
	GLint textureEnvScaleLoc = -1;

	// Uniform of PICA registers
	GLint picaRegLoc = -1;

	// Depth configuration uniform locations
	GLint depthOffsetLoc = -1;
	GLint depthScaleLoc = -1;
	GLint depthmapEnableLoc = -1;

	float oldDepthScale = -1.0;
	float oldDepthOffset = 0.0;
	bool oldDepthmapEnable = false;

	SurfaceCache<DepthBuffer, 10, true> depthBufferCache;
	SurfaceCache<ColourBuffer, 10, true> colourBufferCache;
	SurfaceCache<Texture, 256, true> textureCache;

	OpenGL::uvec2 fbSize;  // The size of the framebuffer (ie both the colour and depth buffer)'

	u32 colourBufferLoc;                // Location in 3DS VRAM for the colour buffer
	PICA::ColorFmt colourBufferFormat;  // Format of the colours stored in the colour buffer

	// Same for the depth/stencil buffer
	u32 depthBufferLoc;
	PICA::DepthFmt depthBufferFormat;

	// Dummy VAO/VBO for blitting the final output
	OpenGL::VertexArray dummyVAO;
	OpenGL::VertexBuffer dummyVBO;

	OpenGL::Texture screenTexture;
	GLuint lightLUTTextureArray;
	OpenGL::Framebuffer screenFramebuffer;

	OpenGL::Framebuffer getColourFBO();
	OpenGL::Texture getTexture(Texture& tex);

	MAKE_LOG_FUNCTION(log, rendererLogger)
	void setupBlending();
	void bindDepthBuffer();
	void setupTextureEnvState();
	void bindTexturesToSlots();
	void updateLightingLUT();

  public:
	RendererGL(GPU& gpu, const std::array<u32, regNum>& internalRegs) : Renderer(gpu, internalRegs) {}

	void reset();
	void display();                                                                                 // Display the 3DS screen contents to the window
	void initGraphicsContext();                                                                     // Initialize graphics context
	void clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control);                     // Clear a GPU buffer in VRAM
	void displayTransfer(u32 inputAddr, u32 outputAddr, u32 inputSize, u32 outputSize, u32 flags);  // Perform display transfer
	void drawVertices(PICA::PrimType primType, std::span<const PICA::Vertex> vertices);             // Draw the given vertices

	// Take a screenshot of the screen and store it in a file
	void screenshot(const std::string& name);

	void setFBSize(u32 width, u32 height) {
		fbSize.x() = width;
		fbSize.y() = height;
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