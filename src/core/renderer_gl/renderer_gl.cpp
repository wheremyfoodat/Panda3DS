#include "renderer_gl/renderer_gl.hpp"

#include <stb_image_write.h>

#include <cmrc/cmrc.hpp>

#include "PICA/float_types.hpp"
#include "PICA/gpu.hpp"
#include "PICA/regs.hpp"

CMRC_DECLARE(RendererGL);

using namespace Floats;
using namespace Helpers;
using namespace PICA;

RendererGL::~RendererGL() {}

void RendererGL::reset() {
	depthBufferCache.reset();
	colourBufferCache.reset();
	textureCache.reset();

	// Init the colour/depth buffer settings to some random defaults on reset
	colourBufferLoc = 0;
	colourBufferFormat = PICA::ColorFmt::RGBA8;

	depthBufferLoc = 0;
	depthBufferFormat = PICA::DepthFmt::Depth16;

	if (triangleProgram.exists()) {
		const auto oldProgram = OpenGL::getProgram();

		gl.useProgram(triangleProgram);

		oldDepthScale = -1.0;       // Default depth scale to -1.0, which is what games typically use
		oldDepthOffset = 0.0;       // Default depth offset to 0
		oldDepthmapEnable = false;  // Enable w buffering

		glUniform1f(depthScaleLoc, oldDepthScale);
		glUniform1f(depthOffsetLoc, oldDepthOffset);
		glUniform1i(depthmapEnableLoc, oldDepthmapEnable);

		gl.useProgram(oldProgram);  // Switch to old GL program
	}
}

void RendererGL::initGraphicsContext(SDL_Window* window) {
	gl.reset();

	auto gl_resources = cmrc::RendererGL::get_filesystem();

	auto vertexShaderSource = gl_resources.open("opengl_vertex_shader.vert");
	auto fragmentShaderSource = gl_resources.open("opengl_fragment_shader.frag");

	OpenGL::Shader vert({vertexShaderSource.begin(), vertexShaderSource.size()}, OpenGL::Vertex);
	OpenGL::Shader frag({fragmentShaderSource.begin(), fragmentShaderSource.size()}, OpenGL::Fragment);
	triangleProgram.create({vert, frag});
	gl.useProgram(triangleProgram);

	textureEnvSourceLoc = OpenGL::uniformLocation(triangleProgram, "u_textureEnvSource");
	textureEnvOperandLoc = OpenGL::uniformLocation(triangleProgram, "u_textureEnvOperand");
	textureEnvCombinerLoc = OpenGL::uniformLocation(triangleProgram, "u_textureEnvCombiner");
	textureEnvColorLoc = OpenGL::uniformLocation(triangleProgram, "u_textureEnvColor");
	textureEnvScaleLoc = OpenGL::uniformLocation(triangleProgram, "u_textureEnvScale");

	depthScaleLoc = OpenGL::uniformLocation(triangleProgram, "u_depthScale");
	depthOffsetLoc = OpenGL::uniformLocation(triangleProgram, "u_depthOffset");
	depthmapEnableLoc = OpenGL::uniformLocation(triangleProgram, "u_depthmapEnable");
	picaRegLoc = OpenGL::uniformLocation(triangleProgram, "u_picaRegs");

	// Init sampler objects. Texture 0 goes in texture unit 0, texture 1 in TU 1, texture 2 in TU 2, and the light maps go in TU 3
	glUniform1i(OpenGL::uniformLocation(triangleProgram, "u_tex0"), 0);
	glUniform1i(OpenGL::uniformLocation(triangleProgram, "u_tex1"), 1);
	glUniform1i(OpenGL::uniformLocation(triangleProgram, "u_tex2"), 2);
	glUniform1i(OpenGL::uniformLocation(triangleProgram, "u_tex_lighting_lut"), 3);

	auto displayVertexShaderSource = gl_resources.open("opengl_display.vert");
	auto displayFragmentShaderSource = gl_resources.open("opengl_display.frag");

	OpenGL::Shader vertDisplay({displayVertexShaderSource.begin(), displayVertexShaderSource.size()}, OpenGL::Vertex);
	OpenGL::Shader fragDisplay({displayFragmentShaderSource.begin(), displayFragmentShaderSource.size()}, OpenGL::Fragment);
	displayProgram.create({vertDisplay, fragDisplay});

	gl.useProgram(displayProgram);
	glUniform1i(OpenGL::uniformLocation(displayProgram, "u_texture"), 0);  // Init sampler object

	vbo.createFixedSize(sizeof(Vertex) * vertexBufferSize, GL_STREAM_DRAW);
	gl.bindVBO(vbo);
	vao.create();
	gl.bindVAO(vao);

	// Position (x, y, z, w) attributes
	vao.setAttributeFloat<float>(0, 4, sizeof(Vertex), offsetof(Vertex, s.positions));
	vao.enableAttribute(0);
	// Quaternion attribute
	vao.setAttributeFloat<float>(1, 4, sizeof(Vertex), offsetof(Vertex, s.quaternion));
	vao.enableAttribute(1);
	// Colour attribute
	vao.setAttributeFloat<float>(2, 4, sizeof(Vertex), offsetof(Vertex, s.colour));
	vao.enableAttribute(2);
	// UV 0 attribute
	vao.setAttributeFloat<float>(3, 2, sizeof(Vertex), offsetof(Vertex, s.texcoord0));
	vao.enableAttribute(3);
	// UV 1 attribute
	vao.setAttributeFloat<float>(4, 2, sizeof(Vertex), offsetof(Vertex, s.texcoord1));
	vao.enableAttribute(4);
	// UV 0 W-component attribute
	vao.setAttributeFloat<float>(5, 1, sizeof(Vertex), offsetof(Vertex, s.texcoord0_w));
	vao.enableAttribute(5);
	// View
	vao.setAttributeFloat<float>(6, 3, sizeof(Vertex), offsetof(Vertex, s.view));
	vao.enableAttribute(6);
	// UV 2 attribute
	vao.setAttributeFloat<float>(7, 2, sizeof(Vertex), offsetof(Vertex, s.texcoord2));
	vao.enableAttribute(7);

	dummyVBO.create();
	dummyVAO.create();

	// Create texture and framebuffer for the 3DS screen
	const u32 screenTextureWidth = 400;       // Top screen is 400 pixels wide, bottom is 320
	const u32 screenTextureHeight = 2 * 240;  // Both screens are 240 pixels tall

	glGenTextures(1, &lightLUTTextureArray);

	auto prevTexture = OpenGL::getTex2D();
	screenTexture.create(screenTextureWidth, screenTextureHeight, GL_RGBA8);
	screenTexture.bind();
	screenTexture.setMinFilter(OpenGL::Linear);
	screenTexture.setMagFilter(OpenGL::Linear);
	glBindTexture(GL_TEXTURE_2D, prevTexture);

	screenFramebuffer.createWithDrawTexture(screenTexture);
	screenFramebuffer.bind(OpenGL::DrawAndReadFramebuffer);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) Helpers::panic("Incomplete framebuffer");

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
void RendererGL::setupBlending() {
	const bool blendingEnabled = (regs[PICA::InternalRegs::ColourOperation] & (1 << 8)) != 0;

	// Map of PICA blending equations to OpenGL blending equations. The unused blending equations are equivalent to equation 0 (add)
	static constexpr std::array<GLenum, 8> blendingEquations = {
		GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT, GL_MIN, GL_MAX, GL_FUNC_ADD, GL_FUNC_ADD, GL_FUNC_ADD,
	};

	// Map of PICA blending funcs to OpenGL blending funcs. Func = 15 is undocumented and stubbed to GL_ONE for now
	static constexpr std::array<GLenum, 16> blendingFuncs = {
		GL_ZERO,
		GL_ONE,
		GL_SRC_COLOR,
		GL_ONE_MINUS_SRC_COLOR,
		GL_DST_COLOR,
		GL_ONE_MINUS_DST_COLOR,
		GL_SRC_ALPHA,
		GL_ONE_MINUS_SRC_ALPHA,
		GL_DST_ALPHA,
		GL_ONE_MINUS_DST_ALPHA,
		GL_CONSTANT_COLOR,
		GL_ONE_MINUS_CONSTANT_COLOR,
		GL_CONSTANT_ALPHA,
		GL_ONE_MINUS_CONSTANT_ALPHA,
		GL_SRC_ALPHA_SATURATE,
		GL_ONE,
	};

	if (!blendingEnabled) {
		gl.disableBlend();
	} else {
		gl.enableBlend();

		// Get blending equations
		const u32 blendControl = regs[PICA::InternalRegs::BlendFunc];
		const u32 rgbEquation = blendControl & 0x7;
		const u32 alphaEquation = getBits<8, 3>(blendControl);

		// Get blending functions
		const u32 rgbSourceFunc = getBits<16, 4>(blendControl);
		const u32 rgbDestFunc = getBits<20, 4>(blendControl);
		const u32 alphaSourceFunc = getBits<24, 4>(blendControl);
		const u32 alphaDestFunc = getBits<28, 4>(blendControl);

		const u32 constantColor = regs[PICA::InternalRegs::BlendColour];
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

void RendererGL::setupTextureEnvState() {
	// TODO: Only update uniforms when the TEV config changed. Use an UBO potentially.

	static constexpr std::array<u32, 6> ioBases = {
		PICA::InternalRegs::TexEnv0Source, PICA::InternalRegs::TexEnv1Source, PICA::InternalRegs::TexEnv2Source,
		PICA::InternalRegs::TexEnv3Source, PICA::InternalRegs::TexEnv4Source, PICA::InternalRegs::TexEnv5Source,
	};

	u32 textureEnvSourceRegs[6];
	u32 textureEnvOperandRegs[6];
	u32 textureEnvCombinerRegs[6];
	u32 textureEnvColourRegs[6];
	u32 textureEnvScaleRegs[6];

	for (int i = 0; i < 6; i++) {
		const u32 ioBase = ioBases[i];

		textureEnvSourceRegs[i] = regs[ioBase];
		textureEnvOperandRegs[i] = regs[ioBase + 1];
		textureEnvCombinerRegs[i] = regs[ioBase + 2];
		textureEnvColourRegs[i] = regs[ioBase + 3];
		textureEnvScaleRegs[i] = regs[ioBase + 4];
	}

	glUniform1uiv(textureEnvSourceLoc, 6, textureEnvSourceRegs);
	glUniform1uiv(textureEnvOperandLoc, 6, textureEnvOperandRegs);
	glUniform1uiv(textureEnvCombinerLoc, 6, textureEnvCombinerRegs);
	glUniform1uiv(textureEnvColorLoc, 6, textureEnvColourRegs);
	glUniform1uiv(textureEnvScaleLoc, 6, textureEnvScaleRegs);
}

void RendererGL::bindTexturesToSlots() {
	static constexpr std::array<u32, 3> ioBases = {
		PICA::InternalRegs::Tex0BorderColor,
		PICA::InternalRegs::Tex1BorderColor,
		PICA::InternalRegs::Tex2BorderColor,
	};

	for (int i = 0; i < 3; i++) {
		if ((regs[PICA::InternalRegs::TexUnitCfg] & (1 << i)) == 0) {
			continue;
		}

		const size_t ioBase = ioBases[i];

		const u32 dim = regs[ioBase + 1];
		const u32 config = regs[ioBase + 2];
		const u32 height = dim & 0x7ff;
		const u32 width = getBits<16, 11>(dim);
		const u32 addr = (regs[ioBase + 4] & 0x0FFFFFFF) << 3;
		u32 format = regs[ioBase + (i == 0 ? 13 : 5)] & 0xF;

		glActiveTexture(GL_TEXTURE0 + i);
		Texture targetTex(addr, static_cast<PICA::TextureFmt>(format), width, height, config);
		OpenGL::Texture tex = getTexture(targetTex);
		tex.bind();
	}

	glActiveTexture(GL_TEXTURE0 + 3);
	glBindTexture(GL_TEXTURE_1D_ARRAY, lightLUTTextureArray);
	glActiveTexture(GL_TEXTURE0);
}

void RendererGL::updateLightingLUT() {
	gpu.lightingLUTDirty = false;
	std::array<u16, GPU::LightingLutSize> u16_lightinglut;

	for (int i = 0; i < gpu.lightingLUT.size(); i++) {
		uint64_t value = gpu.lightingLUT[i] & ((1 << 12) - 1);
		u16_lightinglut[i] = value * 65535 / 4095;
	}

	glActiveTexture(GL_TEXTURE0 + 3);
	glBindTexture(GL_TEXTURE_1D_ARRAY, lightLUTTextureArray);
	glTexImage2D(GL_TEXTURE_1D_ARRAY, 0, GL_R16, 256, Lights::LUT_Count, 0, GL_RED, GL_UNSIGNED_SHORT, u16_lightinglut.data());
	glTexParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glActiveTexture(GL_TEXTURE0);
}

void RendererGL::drawVertices(PICA::PrimType primType, std::span<const Vertex> vertices) {
	// The fourth type is meant to be "Geometry primitive". TODO: Find out what that is
	static constexpr std::array<OpenGL::Primitives, 4> primTypes = {
		OpenGL::Triangle,
		OpenGL::TriangleStrip,
		OpenGL::TriangleFan,
		OpenGL::Triangle,
	};

	const auto primitiveTopology = primTypes[static_cast<usize>(primType)];
	gl.disableScissor();
	gl.bindVBO(vbo);
	gl.bindVAO(vao);
	gl.useProgram(triangleProgram);

	OpenGL::enableClipPlane(0);  // Clipping plane 0 is always enabled
	if (regs[PICA::InternalRegs::ClipEnable] & 1) {
		OpenGL::enableClipPlane(1);
	}

	setupBlending();
	OpenGL::Framebuffer poop = getColourFBO();
	poop.bind(OpenGL::DrawAndReadFramebuffer);

	const u32 depthControl = regs[PICA::InternalRegs::DepthAndColorMask];
	const bool depthEnable = depthControl & 1;
	const bool depthWriteEnable = getBit<12>(depthControl);
	const int depthFunc = getBits<4, 3>(depthControl);
	const int colourMask = getBits<8, 4>(depthControl);
	gl.setColourMask(colourMask & 1, colourMask & 2, colourMask & 4, colourMask & 8);

	static constexpr std::array<GLenum, 8> depthModes = {GL_NEVER, GL_ALWAYS, GL_EQUAL, GL_NOTEQUAL, GL_LESS, GL_LEQUAL, GL_GREATER, GL_GEQUAL};

	const float depthScale = f24::fromRaw(regs[PICA::InternalRegs::DepthScale] & 0xffffff).toFloat32();
	const float depthOffset = f24::fromRaw(regs[PICA::InternalRegs::DepthOffset] & 0xffffff).toFloat32();
	const bool depthMapEnable = regs[PICA::InternalRegs::DepthmapEnable] & 1;

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

	setupTextureEnvState();
	bindTexturesToSlots();

	// Upload PICA Registers as a single uniform. The shader needs access to the rasterizer registers (for depth, starting from index 0x48)
	// The texturing and the fragment lighting registers. Therefore we upload them all in one go to avoid multiple slow uniform updates
	glUniform1uiv(picaRegLoc, 0x200 - 0x48, &regs[0x48]);

	if (gpu.lightingLUTDirty) {
		updateLightingLUT();
	}

	// TODO: Actually use this
	GLsizei viewportWidth = GLsizei(f24::fromRaw(regs[PICA::InternalRegs::ViewportWidth] & 0xffffff).toFloat32() * 2.0f);
	GLsizei viewportHeight = GLsizei(f24::fromRaw(regs[PICA::InternalRegs::ViewportHeight] & 0xffffff).toFloat32() * 2.0f);
	OpenGL::setViewport(viewportWidth, viewportHeight);

	// Note: The code below must execute after we've bound the colour buffer & its framebuffer
	// Because it attaches a depth texture to the aforementioned colour buffer
	if (depthEnable) {
		gl.enableDepth();
		gl.setDepthMask(depthWriteEnable ? GL_TRUE : GL_FALSE);
		gl.setDepthFunc(depthModes[depthFunc]);
		bindDepthBuffer();
	} else {
		if (depthWriteEnable) {
			gl.enableDepth();
			gl.setDepthMask(GL_TRUE);
			gl.setDepthFunc(GL_ALWAYS);
			bindDepthBuffer();
		} else {
			gl.disableDepth();
		}
	}

	vbo.bufferVertsSub(vertices);
	OpenGL::draw(primitiveTopology, GLsizei(vertices.size()));
}

constexpr u32 topScreenBuffer = 0x1f000000;
constexpr u32 bottomScreenBuffer = 0x1f05dc00;

void RendererGL::display() {
	gl.disableScissor();

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	screenFramebuffer.bind(OpenGL::ReadFramebuffer);
	glBlitFramebuffer(0, 0, 400, 480, 0, 0, 400, 480, GL_COLOR_BUFFER_BIT, GL_LINEAR);
}

void RendererGL::clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) {
	return;
	log("GPU: Clear buffer\nStart: %08X End: %08X\nValue: %08X Control: %08X\n", startAddress, endAddress, value, control);

	const float r = float(getBits<24, 8>(value)) / 255.0f;
	const float g = float(getBits<16, 8>(value)) / 255.0f;
	const float b = float(getBits<8, 8>(value)) / 255.0f;
	const float a = float(value & 0xff) / 255.0f;

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

OpenGL::Framebuffer RendererGL::getColourFBO() {
	// We construct a colour buffer object and see if our cache has any matching colour buffers in it
	//  If not, we allocate a texture & FBO for our framebuffer and store it in the cache
	ColourBuffer sampleBuffer(colourBufferLoc, colourBufferFormat, fbSize[0], fbSize[1]);
	auto buffer = colourBufferCache.find(sampleBuffer);

	if (buffer.has_value()) {
		return buffer.value().get().fbo;
	} else {
		return colourBufferCache.add(sampleBuffer).fbo;
	}
}

void RendererGL::bindDepthBuffer() {
	// Similar logic as the getColourFBO function
	DepthBuffer sampleBuffer(depthBufferLoc, depthBufferFormat, fbSize[0], fbSize[1]);
	auto buffer = depthBufferCache.find(sampleBuffer);
	GLuint tex;

	if (buffer.has_value()) {
		tex = buffer.value().get().texture.m_handle;
	} else {
		tex = depthBufferCache.add(sampleBuffer).texture.m_handle;
	}

	if (PICA::DepthFmt::Depth24Stencil8 != depthBufferFormat) {
		Helpers::panicDev("TODO: Should we remove stencil attachment?");
	}
	auto attachment = depthBufferFormat == PICA::DepthFmt::Depth24Stencil8 ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
	glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, tex, 0);
}

OpenGL::Texture RendererGL::getTexture(Texture& tex) {
	// Similar logic as the getColourFBO/bindDepthBuffer functions
	auto buffer = textureCache.find(tex);

	if (buffer.has_value()) {
		return buffer.value().get().texture;
	} else {
		const auto textureData = std::span{gpu.getPointerPhys<u8>(tex.location), tex.sizeInBytes()};  // Get pointer to the texture data in 3DS memory
		Texture& newTex = textureCache.add(tex);
		newTex.decodeTexture(textureData);

		return newTex.texture;
	}
}

void RendererGL::displayTransfer(u32 inputAddr, u32 outputAddr, u32 inputSize, u32 outputSize, u32 flags) {
	const u32 inputWidth = inputSize & 0xffff;
	const u32 inputGap = inputSize >> 16;

	const u32 outputWidth = outputSize & 0xffff;
	const u32 outputGap = outputSize >> 16;

	auto framebuffer = colourBufferCache.findFromAddress(inputAddr);
	// If there's a framebuffer at this address, use it. Otherwise go back to our old hack and display framebuffer 0
	// Displays are hard I really don't want to try implementing them because getting a fast solution is terrible
	OpenGL::Texture& tex = framebuffer.has_value() ? framebuffer.value().get().texture : colourBufferCache[0].texture;

	tex.bind();
	screenFramebuffer.bind(OpenGL::DrawFramebuffer);

	gl.disableBlend();
	gl.disableDepth();
	gl.disableScissor();
	gl.setColourMask(true, true, true, true);
	gl.useProgram(displayProgram);
	gl.bindVAO(dummyVAO);

	OpenGL::disableClipPlane(0);
	OpenGL::disableClipPlane(1);

	// Hack: Detect whether we are writing to the top or bottom screen by checking output gap and drawing to the proper part of the output texture
	// We consider output gap == 320 to mean bottom, and anything else to mean top
	if (outputGap == 320) {
		OpenGL::setViewport(40, 0, 320, 240);  // Bottom screen viewport
	} else {
		OpenGL::setViewport(0, 240, 400, 240);  // Top screen viewport
	}

	OpenGL::draw(OpenGL::TriangleStrip, 4);  // Actually draw our 3DS screen
}

void RendererGL::screenshot(const std::string& name) {
	constexpr uint width = 400;
	constexpr uint height = 2 * 240;

	std::vector<uint8_t> pixels, flippedPixels;
	pixels.resize(width * height * 4);
	flippedPixels.resize(pixels.size());

	OpenGL::bindScreenFramebuffer();
	glReadPixels(0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, pixels.data());

	// Flip the image vertically
	for (int y = 0; y < height; y++) {
		memcpy(&flippedPixels[y * width * 4], &pixels[(height - y - 1) * width * 4], width * 4);
		// Swap R and B channels
		for (int x = 0; x < width; x++) {
			std::swap(flippedPixels[y * width * 4 + x * 4 + 0], flippedPixels[y * width * 4 + x * 4 + 2]);
			// Set alpha to 0xFF
			flippedPixels[y * width * 4 + x * 4 + 3] = 0xFF;
		}
	}

	stbi_write_png(name.c_str(), width, height, 4, flippedPixels.data(), 0);
}
