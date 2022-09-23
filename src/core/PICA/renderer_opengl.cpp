#include "PICA/gpu.hpp"
#include "opengl.hpp"

// This is all hacked up to display our first triangle

const char* vertexShader = R"(
	#version 420 core
	
	layout (location = 0) in vec4 coords;
	layout (location = 1) in vec4 vertexColour;

	out vec4 colour;

	void main() {
		gl_Position = coords;
		colour = vertexColour;
	}
)";

const char* fragmentShader = R"(
	#version 420 core
	
	in vec4 colour;
	out vec4 fragColour;

	void main() {
		fragColour = colour;
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
	fbo.bind(OpenGL::DrawFramebuffer);

	OpenGL::setViewport(400, 240);
	OpenGL::setClearColor(0.0, 0.0, 0.0, 1.0);
	OpenGL::clearColor();

	OpenGL::Shader vert(vertexShader, OpenGL::Vertex);
	OpenGL::Shader frag(fragmentShader, OpenGL::Fragment);
	triangleProgram.create({ vert, frag });

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
	OpenGL::disableDepth();
	OpenGL::disableScissor();
	OpenGL::setViewport(400, 240);
	fbo.bind(OpenGL::DrawAndReadFramebuffer);

	vbo.bind();
	vao.bind();
	triangleProgram.use();
}

void GPU::drawVertices(OpenGL::Primitives primType, Vertex* vertices, u32 count) {
	vbo.bufferVertsSub(vertices, count);
	OpenGL::draw(primType, count);
}

constexpr u32 topScreenBuffer = 0x1f000000;
constexpr u32 bottomScreenBuffer = 0x1f300000;

// Quick hack to display top screen for now
void GPU::display() {
	OpenGL::disableScissor();
	OpenGL::bindScreenFramebuffer();
	fboTexture.bind();
	displayProgram.use();

	dummyVBO.bind();
	dummyVAO.bind();
	OpenGL::setViewport(0, 240, 400, 240);

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