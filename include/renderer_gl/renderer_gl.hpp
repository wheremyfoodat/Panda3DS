#pragma once
#include <array>
#include "helpers.hpp"
#include "logger.hpp"
#include "opengl.hpp"

struct Vertex {
	OpenGL::vec4 position;
	OpenGL::vec4 colour;
};

class Renderer {
	// OpenGL renderer state
	OpenGL::Framebuffer fbo;
	OpenGL::Texture fboTexture;
	OpenGL::Program triangleProgram;
	OpenGL::Program displayProgram;

	OpenGL::VertexArray vao;
	OpenGL::VertexBuffer vbo;
	GLint alphaControlLoc = -1;
	u32 oldAlphaControl = 0;

	// Dummy VAO/VBO for blitting the final output
	OpenGL::VertexArray dummyVAO;
	OpenGL::VertexBuffer dummyVBO;

	static constexpr u32 regNum = 0x300; // Number of internal PICA registers
	const std::array<u32, regNum>& regs;

	MAKE_LOG_FUNCTION(log, rendererLogger)

public:
	Renderer(const std::array<u32, regNum>& internalRegs) : regs(internalRegs) {}

	void reset();
	void display(); // Display the 3DS screen contents to the window
	void initGraphicsContext(); // Initialize graphics context
	void getGraphicsContext();  // Set up graphics context for rendering
	void clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control); // Clear a GPU buffer in VRAM
	void drawVertices(OpenGL::Primitives primType, Vertex* vertices, u32 count); // Draw the given vertices

	static constexpr u32 vertexBufferSize = 0x1500;
};