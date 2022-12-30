#include "PICA/float_types.hpp"
#include "PICA/gpu.hpp"
#include "PICA/regs.hpp"
#include "opengl.hpp"

using namespace Floats;

// This is all hacked up to display our first triangle

const char* vertexShader = R"(
	#version 420 core
	
	layout (location = 0) in vec4 coords;
	layout (location = 1) in vec4 vertexColour;

	out vec4 colour;

	void main() {
		gl_Position = coords * vec4(1.0, 1.0, -1.0, 1.0);
		colour = vertexColour;
	}
)";

const char* fragmentShader = R"(
	#version 420 core
	
	in vec4 colour;
	out vec4 fragColour;

	uniform uint u_alphaControl;

	void main() {
		fragColour = colour;

		if ((u_alphaControl & 1u) != 0u) { // Check if alpha test is on
			uint func = (u_alphaControl >> 4u) & 7u;
			float reference = float((u_alphaControl >> 8u) & 0xffu) / 255.0;
			float alpha = fragColour.a;

			switch (func) {
				case 0: discard; // Never pass alpha test
				case 1: break;          // Always pass alpha test
				case 2:                 // Pass if equal
					if (alpha != reference)
						discard;
					break;
				case 3:                 // Pass if not equal
					if (alpha == reference)
						discard;
					break;
				case 4:                 // Pass if less than
					if (alpha >= reference)
						discard;
					break;
				case 5:                 // Pass if less than or equal
					if (alpha > reference)
						discard;
					break;
				case 6:                 // Pass if greater than
					if (alpha <= reference)
						discard;
					break;
				case 7:                 // Pass if greater than or equal
					if (alpha < reference)
						discard;
					break;
			}
		}
	}
)";

const char* displayVertexShader = R"(
	#version 420 core
	out vec2 UV;

	void main() {
		const vec4 positions[4] = vec4[](
          vec4(-1.0, 1.0, 1.0, 1.0),    // Top-left
          vec4(1.0, 1.0, 1.0, 1.0),     // Top-right
          vec4(-1.0, -1.0, 1.0, 1.0),   // Bottom-left
          vec4(1.0, -1.0, 1.0, 1.0)     // Bottom-right
        );

		// The 3DS displays both screens' framebuffer rotated 90 deg counter clockwise
		// So we adjust our texcoords accordingly
		const vec2 texcoords[4] = vec2[](
				vec2(1.0, 1.0), // Top-right
				vec2(1.0, 0.0), // Bottom-right
				vec2(0.0, 1.0), // Top-left
				vec2(0.0, 0.0)  // Bottom-left
        );

		gl_Position = positions[gl_VertexID];
        UV = texcoords[gl_VertexID];
	}
)";

const char* displayFragmentShader = R"(
    #version 420 core
    in vec2 UV;
    out vec4 FragColor;

    uniform sampler2D u_texture; // TODO: Properly init this to 0 when I'm not lazy
    void main() {
		FragColor = texture(u_texture, UV);
    }
)";

void GPU::initGraphicsContext() {
	// Set up texture for top screen
	fboTexture.create(400, 240, GL_RGBA8);
	fboTexture.bind();

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	fbo.createWithDrawTexture(fboTexture);
	fbo.bind(OpenGL::DrawAndReadFramebuffer);

	GLuint rbo;
	glGenRenderbuffers(1, &rbo);
	glBindRenderbuffer(GL_RENDERBUFFER, rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 400, 240); // use a single renderbuffer object for both a depth AND stencil buffer.
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo); // now actually attach it
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		Helpers::panic("Incomplete framebuffer");
	//glBindRenderbuffer(GL_RENDERBUFFER, 0);

	OpenGL::setViewport(400, 240);
	OpenGL::setClearColor(0.0, 0.0, 0.0, 1.0);
	OpenGL::clearColor();

	OpenGL::Shader vert(vertexShader, OpenGL::Vertex);
	OpenGL::Shader frag(fragmentShader, OpenGL::Fragment);
	triangleProgram.create({ vert, frag });
	triangleProgram.use();
	
	alphaControlLoc = OpenGL::uniformLocation(triangleProgram, "u_alphaControl");
	glUniform1ui(alphaControlLoc, 0); // Default alpha control to 0

	OpenGL::Shader vertDisplay(displayVertexShader, OpenGL::Vertex);
	OpenGL::Shader fragDisplay(displayFragmentShader, OpenGL::Fragment);
	displayProgram.create({ vertDisplay, fragDisplay });

	vbo.createFixedSize(sizeof(Vertex) * vertexBufferSize, GL_STREAM_DRAW);
	vbo.bind();
	vao.create();
	vao.bind();

	// Position (x, y, z, w) attributes
	vao.setAttributeFloat<float>(0, 4, sizeof(Vertex), offsetof(Vertex, position));
	vao.enableAttribute(0);
	// Colour attribute
	vao.setAttributeFloat<float>(1, 4, sizeof(Vertex), offsetof(Vertex, colour));
	vao.enableAttribute(1);

	dummyVBO.create();
	dummyVAO.create();
}

void GPU::getGraphicsContext() {
	OpenGL::disableScissor();
	OpenGL::setViewport(400, 240);
	fbo.bind(OpenGL::DrawAndReadFramebuffer);

	vbo.bind();
	vao.bind();
	triangleProgram.use();
}

void GPU::drawVertices(OpenGL::Primitives primType, Vertex* vertices, u32 count) {
	// Adjust alpha test if necessary
	const u32 alphaControl = regs[PICAInternalRegs::AlphaTestConfig];
	if (alphaControl != oldAlphaControl) {
		oldAlphaControl = alphaControl;
		glUniform1ui(alphaControlLoc, alphaControl);
	}

	const u32 depthControl = regs[PICAInternalRegs::DepthAndColorMask];
	bool depthEnable = depthControl & 1;
	bool depthWriteEnable = (depthControl >> 12) & 1;
	int depthFunc = (depthControl >> 4) & 7;
	int colourMask = (depthControl >> 8) & 0xf;

	static constexpr std::array<GLenum, 8> depthModes = {
		GL_NEVER, GL_ALWAYS, GL_EQUAL, GL_NOTEQUAL, GL_LESS, GL_LEQUAL, GL_GREATER, GL_GEQUAL
	};

	f24 depthScale = f24::fromRaw(regs[PICAInternalRegs::DepthScale] & 0xffffff);
	f24 depthOffset = f24::fromRaw(regs[PICAInternalRegs::DepthOffset] & 0xffffff);
	printf("Depth enable: %d, func: %d, writeEnable: %d\n", depthEnable, depthFunc, depthWriteEnable);

	if (depthScale.toFloat32() != -1.0 || depthOffset.toFloat32() != 0.0)
		Helpers::panic("TODO: Implement depth scale/offset. Remove the depth *= -1.0 from vertex shader");

	// TODO: Actually use this
	float viewportWidth = f24::fromRaw(regs[PICAInternalRegs::ViewportWidth] & 0xffffff).toFloat32() * 2.0;
	float viewportHeight = f24::fromRaw(regs[PICAInternalRegs::ViewportHeight] & 0xffffff).toFloat32() * 2.0;

	if (depthEnable) {
		OpenGL::enableDepth();
		glDepthFunc(depthModes[depthFunc]);
		glDepthMask(depthWriteEnable ? GL_TRUE : GL_FALSE);
	} else {
		if (depthWriteEnable) {
			OpenGL::enableDepth();
			glDepthFunc(GL_ALWAYS);
		} else {
			OpenGL::disableDepth();
		}
	}

	if (colourMask != 0xf) Helpers::panic("[PICA] Colour mask = %X != 0xf", colourMask);
	vbo.bufferVertsSub(vertices, count);
	OpenGL::draw(primType, count);
}

constexpr u32 topScreenBuffer = 0x1f000000;
constexpr u32 bottomScreenBuffer = 0x1f05dc00;

// Quick hack to display top screen for now
void GPU::display() {
	OpenGL::disableDepth();
	OpenGL::disableScissor();
	OpenGL::bindScreenFramebuffer();
	fboTexture.bind();
	displayProgram.use();

	dummyVAO.bind();
	OpenGL::setClearColor(0.0, 0.0, 0.0, 1.0); // Clear screen colour
	OpenGL::clearColor();
	OpenGL::setViewport(0, 240, 400, 240); // Actually draw our 3DS screen
	OpenGL::draw(OpenGL::TriangleStrip, 4);
}

void GPU::clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) {
	log("GPU: Clear buffer\nStart: %08X End: %08X\nValue: %08X Control: %08X\n", startAddress, endAddress, value, control);

	const float r = float((value >> 24) & 0xff) / 255.0;
	const float g = float((value >> 16) & 0xff) / 255.0;
	const float b = float((value >> 8) & 0xff) / 255.0;
	const float a = float(value & 0xff) / 255.0;

	if (startAddress == topScreenBuffer) {
		log("GPU: Cleared top screen\n");
	} else if (startAddress == bottomScreenBuffer) {
		log("GPU: Tried to clear bottom screen\n");
		return;
	} else {
		log("GPU: Clearing some unknown buffer\n");
	}

	OpenGL::setClearColor(r, g, b, a);
	OpenGL::clearColor();
}