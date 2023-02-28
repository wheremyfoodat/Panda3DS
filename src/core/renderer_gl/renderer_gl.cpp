#include "renderer_gl/renderer_gl.hpp"
#include "PICA/float_types.hpp"
#include "PICA/gpu.hpp"
#include "PICA/regs.hpp"

using namespace Floats;

// This is all hacked up to display our first triangle

const char* vertexShader = R"(
	#version 420 core
	
	layout (location = 0) in vec4 coords;
	layout (location = 1) in vec4 vertexColour;
	layout (location = 2) in vec2 inUVs;

	out vec4 colour;
	out vec2 UVs;

	void main() {
		gl_Position = coords * vec4(1.0, 1.0, -1.0, 1.0);
		colour = vertexColour;
		UVs = inUVs;
	}
)";

const char* fragmentShader = R"(
	#version 420 core
	
	in vec4 colour;
	in vec2 UVs;

	out vec4 fragColour;

	uniform uint u_alphaControl;
	uniform uint u_textureConfig;

	uniform sampler2D u_tex0;

	void main() {
		if ((u_textureConfig & 1u) != 0) { // Render texture 0 if enabled
			fragColour = texture(u_tex0, UVs);
		} else {
			fragColour = colour;
		}

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

void Renderer::reset() {
	depthBufferCache.reset();
	colourBufferCache.reset();
	textureCache.reset();

	// Init the colour/depth buffer settings to some random defaults on reset
	colourBufferLoc = 0;
	colourBufferFormat = ColourBuffer::Formats::RGBA8;

	depthBufferLoc = 0;
	depthBufferFormat = DepthBuffer::Formats::Depth16;

	if (triangleProgram.exists()) {
		const auto oldProgram = OpenGL::getProgram();

		triangleProgram.use();
		oldAlphaControl = 0;
		oldTexUnitConfig = 0;

		glUniform1ui(alphaControlLoc, 0); // Default alpha control to 0
		glUniform1ui(texUnitConfigLoc, 0); // Default tex unit config to 0
		glUseProgram(oldProgram); // Switch to old GL program
	}
}

void Renderer::initGraphicsContext() {
	OpenGL::Shader vert(vertexShader, OpenGL::Vertex);
	OpenGL::Shader frag(fragmentShader, OpenGL::Fragment);
	triangleProgram.create({ vert, frag });
	triangleProgram.use();
	
	alphaControlLoc = OpenGL::uniformLocation(triangleProgram, "u_alphaControl");
	texUnitConfigLoc = OpenGL::uniformLocation(triangleProgram, "u_textureConfig");

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
	// UV attribute
	vao.setAttributeFloat<float>(2, 2, sizeof(Vertex), offsetof(Vertex, UVs));
	vao.enableAttribute(2);

	dummyVBO.create();
	dummyVAO.create();
	reset();
}

void Renderer::getGraphicsContext() {
	OpenGL::disableScissor();
	OpenGL::setViewport(400, 240);

	vbo.bind();
	vao.bind();
	triangleProgram.use();
}

// Set up the OpenGL blending context to match the emulated PICA
void Renderer::setupBlending() {
	const bool blendingEnabled = (regs[PICAInternalRegs::ColourOperation] & (1 << 8)) != 0;
	
	// Map of PICA blending equations to OpenGL blending equations. The unused blending equations are equivalent to equation 0 (add)
	static constexpr std::array<GLenum, 8> blendingEquations = {
		GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT, GL_MIN, GL_MAX, GL_FUNC_ADD, GL_FUNC_ADD, GL_FUNC_ADD
	};
	
	// Map of PICA blending funcs to OpenGL blending funcs. Func = 15 is undocumented and stubbed to GL_ONE for now
	static constexpr std::array<GLenum, 16> blendingFuncs = {
		GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
		GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_CONSTANT_COLOR, GL_ONE_MINUS_CONSTANT_COLOR, GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA,
		GL_SRC_ALPHA_SATURATE, GL_ONE
	};

	// Temporarily here until we add constant color/alpha
	const auto panicIfUnimplementedFunc = [](const u32 func) {
		auto x = blendingFuncs[func];
		if (x == GL_CONSTANT_COLOR || x == GL_ONE_MINUS_CONSTANT_COLOR || x == GL_ALPHA || x == GL_ONE_MINUS_CONSTANT_ALPHA) [[unlikely]]
			Helpers::panic("Unimplemented blending function!");
	};

	if (!blendingEnabled) {
		OpenGL::disableBlend();
	} else {
		OpenGL::enableBlend();

		// Get blending equations
		const u32 blendControl = regs[PICAInternalRegs::BlendFunc];
		const u32 rgbEquation = blendControl & 0x7;
		const u32 alphaEquation = (blendControl >> 8) & 0x7;

		// Get blending functions
		const u32 rgbSourceFunc = (blendControl >> 16) & 0xf;
		const u32 rgbDestFunc = (blendControl >> 20) & 0xf;
		const u32 alphaSourceFunc = (blendControl >> 24) & 0xf;
		const u32 alphaDestFunc = (blendControl >> 28) & 0xf;

		// Panic if one of the blending funcs is unimplemented
		panicIfUnimplementedFunc(rgbSourceFunc);
		panicIfUnimplementedFunc(rgbDestFunc);
		panicIfUnimplementedFunc(alphaSourceFunc);
		panicIfUnimplementedFunc(alphaDestFunc);

		// Translate equations and funcs to their GL equivalents and set them
		glBlendEquationSeparate(blendingEquations[rgbEquation], blendingEquations[alphaEquation]);
		glBlendFuncSeparate(blendingFuncs[rgbSourceFunc], blendingFuncs[rgbDestFunc], blendingFuncs[alphaSourceFunc], blendingFuncs[alphaDestFunc]);
	}
}

void Renderer::drawVertices(OpenGL::Primitives primType, Vertex* vertices, u32 count) {
	// Adjust alpha test if necessary
	const u32 alphaControl = regs[PICAInternalRegs::AlphaTestConfig];
	if (alphaControl != oldAlphaControl) {
		oldAlphaControl = alphaControl;
		glUniform1ui(alphaControlLoc, alphaControl);
	}

	setupBlending();
	OpenGL::Framebuffer poop = getColourFBO();
	poop.bind(OpenGL::DrawAndReadFramebuffer);

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

	//if (depthScale.toFloat32() != -1.0 || depthOffset.toFloat32() != 0.0)
	//	Helpers::panic("TODO: Implement depth scale/offset. Remove the depth *= -1.0 from vertex shader");

	// Hack for rendering texture 1
	if (regs[0x80] & 1) {
		u32 dim = regs[0x82];
		u32 height = dim & 0x7ff;
		u32 width = (dim >> 16) & 0x7ff;
		u32 addr = (regs[0x85] & 0x0FFFFFFF) << 3;
		u32 format = regs[0x8E] & 0xF;

		Texture targetTex(addr, static_cast<Texture::Formats>(format), width, height);
		OpenGL::Texture tex = getTexture(targetTex);
		tex.bind();
	}

	// Update the texture unit configuration uniform if it changed
	const u32 texUnitConfig = regs[PICAInternalRegs::TexUnitCfg];
	if (oldTexUnitConfig != texUnitConfig) {
		oldTexUnitConfig = texUnitConfig;
		glUniform1ui(texUnitConfigLoc, texUnitConfig);
	}

	// TODO: Actually use this
	float viewportWidth = f24::fromRaw(regs[PICAInternalRegs::ViewportWidth] & 0xffffff).toFloat32() * 2.0;
	float viewportHeight = f24::fromRaw(regs[PICAInternalRegs::ViewportHeight] & 0xffffff).toFloat32() * 2.0;
	OpenGL::setViewport(viewportWidth, viewportHeight);

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
void Renderer::display() {
	OpenGL::disableDepth();
	OpenGL::disableScissor();

	OpenGL::bindScreenFramebuffer();
	colourBufferCache[0].texture.bind();

	displayProgram.use();

	dummyVAO.bind();
	OpenGL::setClearColor(0.0, 0.0, 1.0, 1.0); // Clear screen colour
	OpenGL::clearColor();
	OpenGL::setViewport(0, 240, 400, 240); // Actually draw our 3DS screen
	OpenGL::draw(OpenGL::TriangleStrip, 4);
}

void Renderer::clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) {
	return;
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

OpenGL::Framebuffer Renderer::getColourFBO() {
	//We construct a colour buffer object and see if our cache has any matching colour buffers in it
	// If not, we allocate a texture & FBO for our framebuffer and store it in the cache 
	ColourBuffer sampleBuffer(colourBufferLoc, colourBufferFormat, fbSize.x(), fbSize.y());
	auto buffer = colourBufferCache.find(sampleBuffer);

	if (buffer.has_value()) {
		return buffer.value().get().fbo;
	} else {
		return colourBufferCache.add(sampleBuffer).fbo;
	}
}

OpenGL::Texture Renderer::getTexture(Texture& tex) {
	// Similar logic as the getColourFBO/getDepthBuffer functions
	auto buffer = textureCache.find(tex);

	if (buffer.has_value()) {
		return buffer.value().get().texture;
	} else {
		const void* textureData = gpu.getPointerPhys<void*>(tex.location); // Get pointer to the texture data in 3DS memory
		Texture& newTex = textureCache.add(tex);
		newTex.decodeTexture(textureData);

		return newTex.texture;
	}
}