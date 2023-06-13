#include "renderer_gl/renderer_gl.hpp"
#include "PICA/float_types.hpp"
#include "PICA/gpu.hpp"
#include "PICA/regs.hpp"

using namespace Floats;
using namespace Helpers;

// This is all hacked up to display our first triangle

const char* vertexShader = R"(
	#version 410 core
	
	layout (location = 0) in vec4 coords;
	layout (location = 1) in vec4 vertexColour;
	layout (location = 2) in vec2 inUVs_texture0;

	out vec4 colour;
	out vec2 tex0_UVs;

	void main() {
		gl_Position = coords;
		colour = vertexColour;

		// Flip y axis of UVs because OpenGL uses an inverted y for texture sampling compared to the PICA
		tex0_UVs = vec2(inUVs_texture0.x, 1.0 - inUVs_texture0.y);
	}
)";

const char* fragmentShader = R"(
	#version 410 core
	
	in vec4 colour;
	in vec2 tex0_UVs;

	out vec4 fragColour;

	uniform uint u_alphaControl;
	uniform uint u_textureConfig;

	// Depth control uniforms
	uniform float u_depthScale;
	uniform float u_depthOffset;
	uniform bool u_depthmapEnable;

	uniform sampler2D u_tex0;

	void main() {
		if ((u_textureConfig & 1u) != 0) { // Render texture 0 if enabled
			fragColour = texture(u_tex0, tex0_UVs);
		} else {
			fragColour = colour;
		}

		// Get original depth value by converting from [near, far] = [0, 1] to [-1, 1]
		// We do this by converting to [0, 2] first and subtracting 1 to go to [-1, 1]
		float z_over_w = gl_FragCoord.z * 2.0f - 1.0f;
		float depth = z_over_w * u_depthScale + u_depthOffset;

		if (!u_depthmapEnable) // Divide z by w if depthmap enable == 0 (ie using W-buffering)
			depth /= gl_FragCoord.w;

		// Write final fragment depth
		gl_FragDepth = depth;

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
	#version 410 core
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
    #version 410 core
    in vec2 UV;
    out vec4 FragColor;

    uniform sampler2D u_texture;
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
		oldAlphaControl = 0; // Default alpha control to 0
		oldTexUnitConfig = 0; // Default tex unit config to 0
		
		oldDepthScale = -1.0; // Default depth scale to -1.0, which is what games typically use
		oldDepthOffset = 0.0; // Default depth offset to 0
		oldDepthmapEnable = false; // Enable w buffering

		glUniform1ui(alphaControlLoc, oldAlphaControl);
		glUniform1ui(texUnitConfigLoc, oldTexUnitConfig);
		glUniform1f(depthScaleLoc, oldDepthScale);
		glUniform1f(depthOffsetLoc, oldDepthOffset);
		glUniform1i(depthmapEnableLoc, oldDepthmapEnable);

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

	depthScaleLoc = OpenGL::uniformLocation(triangleProgram, "u_depthScale");
	depthOffsetLoc = OpenGL::uniformLocation(triangleProgram, "u_depthOffset");
	depthmapEnableLoc = OpenGL::uniformLocation(triangleProgram, "u_depthmapEnable");
	glUniform1i(OpenGL::uniformLocation(triangleProgram, "u_tex0"), 0); // Init sampler object

	OpenGL::Shader vertDisplay(displayVertexShader, OpenGL::Vertex);
	OpenGL::Shader fragDisplay(displayFragmentShader, OpenGL::Fragment);
	displayProgram.create({ vertDisplay, fragDisplay });

	displayProgram.use();
	glUniform1i(OpenGL::uniformLocation(displayProgram, "u_texture"), 0); // Init sampler object

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

	// Create texture and framebuffer for the 3DS screen
	const u32 screenTextureWidth = 2 * 400; // Top screen is 400 pixels wide, bottom is 320
	const u32 screenTextureHeight = 2 * 240; // Both screens are 240 pixels tall

	auto prevTexture = OpenGL::getTex2D();
	screenTexture.create(screenTextureWidth, screenTextureHeight, GL_RGBA8);
	screenTexture.bind();
	screenTexture.setMinFilter(OpenGL::Linear);
	screenTexture.setMagFilter(OpenGL::Linear);
	glBindTexture(GL_TEXTURE_2D, prevTexture);

	screenFramebuffer.createWithDrawTexture(screenTexture);
	screenFramebuffer.bind(OpenGL::DrawAndReadFramebuffer);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		Helpers::panic("Incomplete framebuffer");

	// TODO: This should not clear the framebuffer contents. It should load them from VRAM.
	GLint oldViewport[4];
	glGetIntegerv(GL_VIEWPORT, oldViewport);
	OpenGL::setViewport(screenTextureWidth, screenTextureHeight);
	OpenGL::setClearColor(0.0, 0.0, 0.0, 1.0);
	OpenGL::clearColor();
	OpenGL::setViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);

	reset();
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

	if (!blendingEnabled) {
		OpenGL::disableBlend();
	} else {
		OpenGL::enableBlend();

		// Get blending equations
		const u32 blendControl = regs[PICAInternalRegs::BlendFunc];
		const u32 rgbEquation = blendControl & 0x7;
		const u32 alphaEquation = getBits<8, 3>(blendControl);

		// Get blending functions
		const u32 rgbSourceFunc = getBits<16, 4>(blendControl);
		const u32 rgbDestFunc = getBits<20, 4>(blendControl);
		const u32 alphaSourceFunc = getBits<24, 4>(blendControl);
		const u32 alphaDestFunc = getBits<28, 4>(blendControl);

		const u32 constantColor = regs[PICAInternalRegs::BlendColour];
		const u32 r = constantColor & 0xff;
		const u32 g = getBits<8, 8>(constantColor);
		const u32 b = getBits<16, 8>(constantColor);
		const u32 a = getBits<24, 8>(constantColor);
		OpenGL::setBlendColor(float(r) / 255.f, float(g) / 255.f, float(b) / 255.f, float(a) / 255.f);

		// Translate equations and funcs to their GL equivalents and set them
		glBlendEquationSeparate(blendingEquations[rgbEquation], blendingEquations[alphaEquation]);
		glBlendFuncSeparate(blendingFuncs[rgbSourceFunc], blendingFuncs[rgbDestFunc], blendingFuncs[alphaSourceFunc], blendingFuncs[alphaDestFunc]);
	}
}

void Renderer::drawVertices(OpenGL::Primitives primType, Vertex* vertices, u32 count) {
	OpenGL::disableScissor();

	vbo.bind();
	vao.bind();
	triangleProgram.use();

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
	const bool depthEnable = depthControl & 1;
	const bool depthWriteEnable = getBit<12>(depthControl);
	const int depthFunc = getBits<4, 3>(depthControl);
	const int colourMask = getBits<8, 4>(depthControl);
	glColorMask(colourMask & 1, colourMask & 2, colourMask & 4, colourMask & 8);

	static constexpr std::array<GLenum, 8> depthModes = {
		GL_NEVER, GL_ALWAYS, GL_EQUAL, GL_NOTEQUAL, GL_LESS, GL_LEQUAL, GL_GREATER, GL_GEQUAL
	};

	const float depthScale = f24::fromRaw(regs[PICAInternalRegs::DepthScale] & 0xffffff).toFloat32();
	const float depthOffset = f24::fromRaw(regs[PICAInternalRegs::DepthOffset] & 0xffffff).toFloat32();
	const bool depthMapEnable = regs[PICAInternalRegs::DepthmapEnable] & 1;

	// Update depth uniforms
	if (oldDepthScale != depthScale) {
		oldDepthScale = depthScale;
		glUniform1f(depthScaleLoc, depthScale);
	}
	
	if (oldDepthOffset != depthOffset) {
		oldDepthOffset = depthOffset;
		glUniform1f(depthOffsetLoc, depthOffset);
	}

	if (oldDepthmapEnable != depthMapEnable) {
		oldDepthmapEnable = depthMapEnable;
		glUniform1i(depthmapEnableLoc, depthMapEnable);
	}

	// Hack for rendering texture 1
	if (regs[0x80] & 1) {
		const u32 dim = regs[0x82];
		const u32 config = regs[0x83];
		const u32 height = dim & 0x7ff;
		const u32 width = getBits<16, 11>(dim);
		const u32 addr = (regs[0x85] & 0x0FFFFFFF) << 3;
		const u32 format = regs[0x8E] & 0xF;

		Texture targetTex(addr, static_cast<Texture::Formats>(format), width, height, config);
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

	// Note: The code below must execute after we've bound the colour buffer & its framebuffer
	// Because it attaches a depth texture to the aforementioned colour buffer
	if (depthEnable) {
		OpenGL::enableDepth();
		glDepthFunc(depthModes[depthFunc]);
		glDepthMask(depthWriteEnable ? GL_TRUE : GL_FALSE);
		bindDepthBuffer();
	} else {
		if (depthWriteEnable) {
			OpenGL::enableDepth();
			glDepthFunc(GL_ALWAYS);
			glDepthMask(GL_TRUE);
			bindDepthBuffer();
		} else {
			OpenGL::disableDepth();
		}
	}

	vbo.bufferVertsSub(vertices, count);
	OpenGL::draw(primType, count);
}

constexpr u32 topScreenBuffer = 0x1f000000;
constexpr u32 bottomScreenBuffer = 0x1f05dc00;

// Quick hack to display top screen for now
void Renderer::display() {
	OpenGL::disableScissor();

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	screenFramebuffer.bind(OpenGL::ReadFramebuffer);
	glBlitFramebuffer(0, 0, 400, 480, 0, 0, 400, 480, GL_COLOR_BUFFER_BIT, GL_LINEAR);
}

void Renderer::clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) {
	return;
	log("GPU: Clear buffer\nStart: %08X End: %08X\nValue: %08X Control: %08X\n", startAddress, endAddress, value, control);

	const float r = float(getBits<24, 8>(value)) / 255.0;
	const float g = float(getBits<16, 8>(value)) / 255.0;
	const float b = float(getBits<8, 8>(value)) / 255.0;
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
		printf("New colour buffer: %08X\n", colourBufferLoc);
		return colourBufferCache.add(sampleBuffer).fbo;
	}
}

void Renderer::bindDepthBuffer() {
	// Similar logic as the getColourFBO function
	DepthBuffer sampleBuffer(depthBufferLoc, depthBufferFormat, fbSize.x(), fbSize.y());
	auto buffer = depthBufferCache.find(sampleBuffer);
	GLuint tex;

	if (buffer.has_value()) {
		tex = buffer.value().get().texture.m_handle;
	} else {
		tex = depthBufferCache.add(sampleBuffer).texture.m_handle;
	}

	if (DepthBuffer::Formats::Depth24Stencil8 != depthBufferFormat) Helpers::panic("TODO: Should we remove stencil attachment?");
	auto attachment = depthBufferFormat == DepthBuffer::Formats::Depth24Stencil8 ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
	glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, tex, 0);
}

OpenGL::Texture Renderer::getTexture(Texture& tex) {
	// Similar logic as the getColourFBO/bindDepthBuffer functions
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

void Renderer::displayTransfer(u32 inputAddr, u32 outputAddr, u32 inputSize, u32 outputSize, u32 flags) {
	const u32 inputWidth = inputSize & 0xffff;
	const u32 inputGap = inputSize >> 16;

	const u32 outputWidth = outputSize & 0xffff;
	const u32 outputGap = outputSize >> 16;

	auto framebuffer = colourBufferCache.findFromAddress(inputAddr);
	if (framebuffer.has_value()) {
		OpenGL::Texture& texture = framebuffer.value().get().texture;
		texture.bind();
		screenFramebuffer.bind(OpenGL::DrawFramebuffer);

		OpenGL::disableBlend();
		OpenGL::disableDepth();
		OpenGL::disableScissor();

		displayProgram.use();

		// Hack: Detect whether we are writing to the top or bottom screen by checking output gap and drawing to the proper part of the output texture
		// We consider output gap == 320 to mean bottom, and anything else to mean top
		if (outputGap == 320) {
			OpenGL::setViewport(40, 0, 320, 240);
		} else {
			OpenGL::setViewport(0, 240, 400, 240);
		}

		dummyVAO.bind();
		OpenGL::draw(OpenGL::TriangleStrip, 4); // Actually draw our 3DS screen
	}
}