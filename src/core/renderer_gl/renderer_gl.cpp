#include "renderer_gl/renderer_gl.hpp"

#include <stb_image_write.h>

#include <bit>
#include <cmrc/cmrc.hpp>

#include "PICA/float_types.hpp"
#include "PICA/gpu.hpp"
#include "PICA/pica_frag_uniforms.hpp"
#include "PICA/pica_simd.hpp"
#include "PICA/regs.hpp"
#include "PICA/shader_decompiler.hpp"
#include "config.hpp"
#include "math_util.hpp"

CMRC_DECLARE(RendererGL);

using namespace Floats;
using namespace Helpers;
using namespace PICA;

RendererGL::~RendererGL() {}

void RendererGL::reset() {
	depthBufferCache.reset();
	colourBufferCache.reset();
	textureCache.reset();

	shaderCache.clear();

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

		glUniform1f(ubershaderData.depthScaleLoc, oldDepthScale);
		glUniform1f(ubershaderData.depthOffsetLoc, oldDepthOffset);
		glUniform1i(ubershaderData.depthmapEnableLoc, oldDepthmapEnable);

		gl.useProgram(oldProgram);  // Switch to old GL program
	}
}

void RendererGL::initGraphicsContextInternal() {
	gl.reset();

	auto gl_resources = cmrc::RendererGL::get_filesystem();
	auto vertexShaderSource = gl_resources.open("opengl_vertex_shader.vert");
	auto fragmentShaderSource = gl_resources.open("opengl_fragment_shader.frag");

	OpenGL::Shader vert({vertexShaderSource.begin(), vertexShaderSource.size()}, OpenGL::Vertex);
	OpenGL::Shader frag({fragmentShaderSource.begin(), fragmentShaderSource.size()}, OpenGL::Fragment);
	triangleProgram.create({vert, frag});
	initUbershader(triangleProgram);

	compileDisplayShader();
	// Create stream buffers for vertex, index and uniform buffers
	static constexpr usize hwIndexBufferSize = 2_MB;
	static constexpr usize hwVertexBufferSize = 16_MB;

	hwIndexBuffer = StreamBuffer::Create(GL_ELEMENT_ARRAY_BUFFER, hwIndexBufferSize);
	hwVertexBuffer = StreamBuffer::Create(GL_ARRAY_BUFFER, hwVertexBufferSize);

	// Allocate memory for the shadergen fragment uniform UBO
	glGenBuffers(1, &shadergenFragmentUBO);
	gl.bindUBO(shadergenFragmentUBO);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(PICA::FragmentUniforms), nullptr, GL_DYNAMIC_DRAW);

	// Allocate memory for the accelerated vertex shader uniform UBO
	glGenBuffers(1, &hwShaderUniformUBO);
	gl.bindUBO(hwShaderUniformUBO);
	glBufferData(GL_UNIFORM_BUFFER, PICAShader::totalUniformSize(), nullptr, GL_DYNAMIC_DRAW);

	vbo.createFixedSize(sizeof(Vertex) * vertexBufferSize * 2, GL_STREAM_DRAW);
	vbo.bind();
	// Initialize the VAO used when not using hw shaders
	defaultVAO.create();
	gl.bindVAO(defaultVAO);

	// Position (x, y, z, w) attributes
	defaultVAO.setAttributeFloat<float>(0, 4, sizeof(Vertex), offsetof(Vertex, s.positions));
	defaultVAO.enableAttribute(0);
	// Quaternion attribute
	defaultVAO.setAttributeFloat<float>(1, 4, sizeof(Vertex), offsetof(Vertex, s.quaternion));
	defaultVAO.enableAttribute(1);
	// Colour attribute
	defaultVAO.setAttributeFloat<float>(2, 4, sizeof(Vertex), offsetof(Vertex, s.colour));
	defaultVAO.enableAttribute(2);
	// UV 0 attribute
	defaultVAO.setAttributeFloat<float>(3, 2, sizeof(Vertex), offsetof(Vertex, s.texcoord0));
	defaultVAO.enableAttribute(3);
	// UV 1 attribute
	defaultVAO.setAttributeFloat<float>(4, 2, sizeof(Vertex), offsetof(Vertex, s.texcoord1));
	defaultVAO.enableAttribute(4);
	// UV 0 W-component attribute
	defaultVAO.setAttributeFloat<float>(5, 1, sizeof(Vertex), offsetof(Vertex, s.texcoord0_w));
	defaultVAO.enableAttribute(5);
	// View
	defaultVAO.setAttributeFloat<float>(6, 3, sizeof(Vertex), offsetof(Vertex, s.view));
	defaultVAO.enableAttribute(6);
	// UV 2 attribute
	defaultVAO.setAttributeFloat<float>(7, 2, sizeof(Vertex), offsetof(Vertex, s.texcoord2));
	defaultVAO.enableAttribute(7);

	// Initialize the VAO used for hw shaders
	hwShaderVAO.create();

	dummyVBO.create();
	dummyVAO.create();
	gl.disableScissor();

	// Create texture and framebuffer for the 3DS screen
	const u32 screenTextureWidth = 400;       // Top screen is 400 pixels wide, bottom is 320
	const u32 screenTextureHeight = 2 * 240;  // Both screens are 240 pixels tall

	// 24 rows for light, 1 for fog
	LUTTexture.create(256, Lights::LUT_Count + 1, GL_RG32F);
	LUTTexture.bind();
	LUTTexture.setMinFilter(OpenGL::Linear);
	LUTTexture.setMagFilter(OpenGL::Linear);

	auto prevTexture = OpenGL::getTex2D();

	// Create a plain black texture for when a game reads an invalid texture. It is common for games to configure the PICA to read texture info from NULL.
	// Some games that do this are Pokemon X, Cars 2, Tomodachi Life, and more. We bind the texture to an FBO, clear it, and free the FBO
	blankTexture.create(8, 8, GL_RGBA8);
	blankTexture.bind();
	blankTexture.setMinFilter(OpenGL::Linear);
	blankTexture.setMagFilter(OpenGL::Linear);

	OpenGL::Framebuffer dummyFBO;
	dummyFBO.createWithDrawTexture(blankTexture);  // Create FBO and bind our texture to it
	dummyFBO.bind(OpenGL::DrawFramebuffer);

	// Clear the texture and then delete FBO
	OpenGL::setViewport(8, 8);
	gl.setClearColour(0.0, 0.0, 0.0, 0.0);
	OpenGL::clearColor();
	dummyFBO.free();

	screenTexture.create(screenTextureWidth, screenTextureHeight, GL_RGBA8);
	screenTexture.bind();
	screenTexture.setMinFilter(OpenGL::Linear);
	screenTexture.setMagFilter(OpenGL::Linear);
	glBindTexture(GL_TEXTURE_2D, prevTexture);

	screenFramebuffer.createWithDrawTexture(screenTexture);
	screenFramebuffer.bind(OpenGL::DrawAndReadFramebuffer);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		Helpers::panic("Incomplete framebuffer");
	}

	// TODO: This should not clear the framebuffer contents. It should load them from VRAM.
	GLint oldViewport[4];
	glGetIntegerv(GL_VIEWPORT, oldViewport);
	OpenGL::setViewport(screenTextureWidth, screenTextureHeight);
	OpenGL::clearColor();
	OpenGL::setViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);

	// Initialize fixed attributes
	for (int i = 0; i < fixedAttrValues.size(); i++) {
		fixedAttrValues[i] = {0.f, 0.f, 0.f, 0.f};
		glVertexAttrib4f(i, 0.0, 0.0, 0.0, 0.0);
	}

	reset();
	fragShaderGen.setTarget(driverInfo.usingGLES ? PICA::ShaderGen::API::GLES : PICA::ShaderGen::API::GL, PICA::ShaderGen::Language::GLSL);

	// Populate our driver info structure
	driverInfo.supportsExtFbFetch = (GLAD_GL_EXT_shader_framebuffer_fetch != 0);
	driverInfo.supportsArmFbFetch = (GLAD_GL_ARM_shader_framebuffer_fetch != 0);

	// Initialize the default vertex shader used with shadergen
	std::string defaultShadergenVSSource = fragShaderGen.getDefaultVertexShader();
	defaultShadergenVs.create({defaultShadergenVSSource.c_str(), defaultShadergenVSSource.size()}, OpenGL::Vertex);
}

// The OpenGL renderer doesn't need to do anything with the GL context (For Qt frontend) or the SDL window (For SDL frontend)
// So we just call initGraphicsContextInternal for both
void RendererGL::initGraphicsContext([[maybe_unused]] SDL_Window* window) { initGraphicsContextInternal(); }

// Set up the OpenGL blending context to match the emulated PICA
void RendererGL::setupBlending() {
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

	static constexpr std::array<GLenum, 16> logicOps = {
		GL_CLEAR, GL_AND, GL_AND_REVERSE, GL_COPY, GL_SET,   GL_COPY_INVERTED, GL_NOOP,       GL_INVERT,
		GL_NAND,  GL_OR,  GL_NOR,         GL_XOR,  GL_EQUIV, GL_AND_INVERTED,  GL_OR_REVERSE, GL_OR_INVERTED,
	};

	// Shows if blending is enabled. If it is not enabled, then logic ops are enabled instead
	const bool blendingEnabled = (regs[PICA::InternalRegs::ColourOperation] & (1 << 8)) != 0;

	if (!blendingEnabled) { // Logic ops are enabled
		const u32 logicOp = getBits<0, 4>(regs[PICA::InternalRegs::LogicOp]);
		gl.setLogicOp(logicOps[logicOp]);

		// If logic ops are enabled we don't need to disable blending because they override it
		gl.enableLogicOp();
	} else {
		gl.enableBlend();
		gl.disableLogicOp();

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
		gl.setBlendEquation(blendingEquations[rgbEquation], blendingEquations[alphaEquation]);
		gl.setBlendFunc(blendingFuncs[rgbSourceFunc], blendingFuncs[rgbDestFunc], blendingFuncs[alphaSourceFunc], blendingFuncs[alphaDestFunc]);
	}
}

void RendererGL::setupStencilTest(bool stencilEnable) {
	if (!stencilEnable) {
		gl.disableStencil();
		return;
	}

	static constexpr std::array<GLenum, 8> stencilFuncs = {
		GL_NEVER,
		GL_ALWAYS,
		GL_EQUAL,
		GL_NOTEQUAL,
		GL_LESS,
		GL_LEQUAL,
		GL_GREATER,
		GL_GEQUAL
	};
	gl.enableStencil();

	const u32 stencilConfig = regs[PICA::InternalRegs::StencilTest];
	const u32 stencilFunc = getBits<4, 3>(stencilConfig);
	const s32 reference = s8(getBits<16, 8>(stencilConfig)); // Signed reference value
	const u32 stencilRefMask = getBits<24, 8>(stencilConfig);

	const bool stencilWrite = regs[PICA::InternalRegs::DepthBufferWrite];
	const u32 stencilBufferMask = stencilWrite ? getBits<8, 8>(stencilConfig) : 0;

	// TODO: Throw stencilFunc/stencilOp to the GL state manager
	glStencilFunc(stencilFuncs[stencilFunc], reference, stencilRefMask);
	gl.setStencilMask(stencilBufferMask);

	static constexpr std::array<GLenum, 8> stencilOps = {
		GL_KEEP,
		GL_ZERO,
		GL_REPLACE,
		GL_INCR,
		GL_DECR,
		GL_INVERT,
		GL_INCR_WRAP,
		GL_DECR_WRAP
	};
	const u32 stencilOpConfig = regs[PICA::InternalRegs::StencilOp];
	const u32 stencilFailOp = getBits<0, 3>(stencilOpConfig);
	const u32 depthFailOp = getBits<4, 3>(stencilOpConfig);
	const u32 passOp = getBits<8, 3>(stencilOpConfig);

	glStencilOp(stencilOps[stencilFailOp], stencilOps[depthFailOp], stencilOps[passOp]);
}

void RendererGL::setupUbershaderTexEnv() {
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

	glUniform1uiv(ubershaderData.textureEnvSourceLoc, 6, textureEnvSourceRegs);
	glUniform1uiv(ubershaderData.textureEnvOperandLoc, 6, textureEnvOperandRegs);
	glUniform1uiv(ubershaderData.textureEnvCombinerLoc, 6, textureEnvCombinerRegs);
	glUniform1uiv(ubershaderData.textureEnvColorLoc, 6, textureEnvColourRegs);
	glUniform1uiv(ubershaderData.textureEnvScaleLoc, 6, textureEnvScaleRegs);
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

		if (addr != 0) [[likely]] {
			Texture targetTex(addr, static_cast<PICA::TextureFmt>(format), width, height, config);
			OpenGL::Texture tex = getTexture(targetTex);
			tex.bind();
		} else {
			// Mapping a texture from NULL. PICA seems to read the last sampled colour, but for now we will display a black texture instead since it is far easier.
			// Games that do this don't really care what it does, they just expect the PICA to not crash, since it doesn't have a PU/MMU and can do all sorts of
			// Weird invalid memory accesses without crashing
			blankTexture.bind();
		}
	}

	glActiveTexture(GL_TEXTURE0 + 3);
	LUTTexture.bind();
	glActiveTexture(GL_TEXTURE0);
}

void RendererGL::updateLightingLUT() {
	gpu.lightingLUTDirty = false;
	std::array<float, GPU::LightingLutSize * 2> lightingLut;

	for (int i = 0; i < lightingLut.size(); i += 2) {
		uint64_t value = gpu.lightingLUT[i >> 1] & 0xFFF;
		lightingLut[i] = (float)(value << 4) / 65535.0f;
	}

	glActiveTexture(GL_TEXTURE0 + 3);
	LUTTexture.bind();
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, Lights::LUT_Count, GL_RG, GL_FLOAT, lightingLut.data());
	glActiveTexture(GL_TEXTURE0);
}

void RendererGL::updateFogLUT() {
	gpu.fogLUTDirty = false;

	// Fog LUT elements are of this type:
	// 0-12     fixed1.1.11, Difference from next element
	// 13-23    fixed0.0.11, Value
	// We will store them as a 128x1 RG texture with R being the value and G being the difference
	std::array<float, 128 * 2> fogLut;

	for (int i = 0; i < fogLut.size(); i += 2) {
		const uint32_t value = gpu.fogLUT[i >> 1];
		int32_t diff = value & 0x1fff;
		diff = (diff << 19) >> 19;  // Sign extend the 13-bit value to 32 bits
		const float fogDifference = float(diff) / 2048.0f;
		const float fogValue = float((value >> 13) & 0x7ff) / 2048.0f;

		fogLut[i] = fogValue;
		fogLut[i + 1] = fogDifference;
	}

	glActiveTexture(GL_TEXTURE0 + 3);
	LUTTexture.bind();
	// The fog LUT exists at the end of the lighting LUT
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, Lights::LUT_Count, 128, 1, GL_RG, GL_FLOAT, fogLut.data());
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

	// If we're using accelerated shaders, the hw VAO, VBO and EBO objects will have already been bound in prepareForDraw
	if (!usingAcceleratedShader) {
		vbo.bind();
		gl.bindVAO(defaultVAO);
	}

	gl.enableClipPlane(0);  // Clipping plane 0 is always enabled
	if (regs[PICA::InternalRegs::ClipEnable] & 1) {
		gl.enableClipPlane(1);
	}

	setupBlending();
	auto poop = getColourBuffer(colourBufferLoc, colourBufferFormat, fbSize[0], fbSize[1]);
	poop->fbo.bind(OpenGL::DrawAndReadFramebuffer);

	const u32 depthControl = regs[PICA::InternalRegs::DepthAndColorMask];
	const bool depthWrite = regs[PICA::InternalRegs::DepthBufferWrite];
	const bool depthEnable = depthControl & 1;
	const bool depthWriteEnable = getBit<12>(depthControl);
	const int depthFunc = getBits<4, 3>(depthControl);
	const int colourMask = getBits<8, 4>(depthControl);
	gl.setColourMask(colourMask & 1, colourMask & 2, colourMask & 4, colourMask & 8);
	static constexpr std::array<GLenum, 8> depthModes = {GL_NEVER, GL_ALWAYS, GL_EQUAL, GL_NOTEQUAL, GL_LESS, GL_LEQUAL, GL_GREATER, GL_GEQUAL};

	bindTexturesToSlots();
	if (gpu.fogLUTDirty) {
		updateFogLUT();
	}

	if (gpu.lightingLUTDirty) {
		updateLightingLUT();
	}

	const GLsizei viewportX = regs[PICA::InternalRegs::ViewportXY] & 0x3ff;
	const GLsizei viewportY = (regs[PICA::InternalRegs::ViewportXY] >> 16) & 0x3ff;
	const GLsizei viewportWidth = GLsizei(f24::fromRaw(regs[PICA::InternalRegs::ViewportWidth] & 0xffffff).toFloat32() * 2.0f);
	const GLsizei viewportHeight = GLsizei(f24::fromRaw(regs[PICA::InternalRegs::ViewportHeight] & 0xffffff).toFloat32() * 2.0f);
	const auto rect = poop->getSubRect(colourBufferLoc, fbSize[0], fbSize[1]);
	OpenGL::setViewport(rect.left + viewportX, rect.bottom + viewportY, viewportWidth, viewportHeight);

	const u32 stencilConfig = regs[PICA::InternalRegs::StencilTest];
	const bool stencilEnable = getBit<0>(stencilConfig);

	// Note: The code below must execute after we've bound the colour buffer & its framebuffer
	// Because it attaches a depth texture to the aforementioned colour buffer
	if (depthEnable) {
		gl.enableDepth();
		gl.setDepthMask(depthWriteEnable && depthWrite ? GL_TRUE : GL_FALSE);
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

			if (stencilEnable) {
				bindDepthBuffer();
			}
		}
	}

	setupStencilTest(stencilEnable);

	if (!usingAcceleratedShader) {
		vbo.bufferVertsSub(vertices);
		OpenGL::draw(primitiveTopology, GLsizei(vertices.size()));
	} else {
		if (performIndexedRender) {
			// When doing indexed rendering, use glDrawRangeElementsBaseVertex to issue the indexed draw
			hwIndexBuffer->Bind();

			if (glDrawRangeElementsBaseVertex != nullptr) [[likely]] {
				glDrawRangeElementsBaseVertex(
					primitiveTopology, minimumIndex, maximumIndex, GLsizei(vertices.size()), usingShortIndices ? GL_UNSIGNED_SHORT : GL_UNSIGNED_BYTE,
					hwIndexBufferOffset, -GLint(minimumIndex)
				);
			} else {
				// If glDrawRangeElementsBaseVertex is not available then prepareForDraw will have subtracted the base vertex from the index buffer
				// for us, so just use glDrawRangeElements
				glDrawRangeElements(
					primitiveTopology, 0, GLint(maximumIndex - minimumIndex), GLsizei(vertices.size()),
					usingShortIndices ? GL_UNSIGNED_SHORT : GL_UNSIGNED_BYTE, hwIndexBufferOffset
				);
			}
		} else {
			// When doing non-indexed rendering, just use glDrawArrays
			OpenGL::draw(primitiveTopology, GLsizei(vertices.size()));
		}
	}
}

void RendererGL::display() {
	gl.disableScissor();
	gl.disableBlend();
	gl.disableDepth();
	// This will work fine whether or not logic ops are enabled. We set logic op to copy instead of disabling to avoid state changes
	gl.setLogicOp(GL_COPY);
	gl.setColourMask(true, true, true, true);
	gl.useProgram(displayProgram);
	gl.bindVAO(dummyVAO);

	gl.disableClipPlane(0);
	gl.disableClipPlane(1);

	screenFramebuffer.bind(OpenGL::DrawFramebuffer);
	gl.setClearColour(0.f, 0.f, 0.f, 1.f);
	OpenGL::clearColor();

	using namespace PICA::ExternalRegs;
	const u32 topActiveFb = externalRegs[Framebuffer0Select] & 1;
	const u32 topScreenAddr = externalRegs[topActiveFb == 0 ? Framebuffer0AFirstAddr : Framebuffer0ASecondAddr];
	auto topScreen = colourBufferCache.findFromAddress(topScreenAddr);

	if (topScreen) {
		topScreen->get().texture.bind();
		OpenGL::setViewport(0, 240, 400, 240); // Top screen viewport
		OpenGL::draw(OpenGL::TriangleStrip, 4); // Actually draw our 3DS screen
	}

	const u32 bottomActiveFb = externalRegs[Framebuffer1Select] & 1;
	const u32 bottomScreenAddr = externalRegs[bottomActiveFb == 0 ? Framebuffer1AFirstAddr : Framebuffer1ASecondAddr];
	auto bottomScreen = colourBufferCache.findFromAddress(bottomScreenAddr);
	
	if (bottomScreen) {
		bottomScreen->get().texture.bind();
		OpenGL::setViewport(40, 0, 320, 240);
		OpenGL::draw(OpenGL::TriangleStrip, 4);
	}

	if constexpr (!Helpers::isHydraCore()) {
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		screenFramebuffer.bind(OpenGL::ReadFramebuffer);
		glBlitFramebuffer(0, 0, 400, 480, 0, 0, outputWindowWidth, outputWindowHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
	}
}

void RendererGL::clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) {
	log("GPU: Clear buffer\nStart: %08X End: %08X\nValue: %08X Control: %08X\n", startAddress, endAddress, value, control);
	gl.disableScissor();

	const auto color = colourBufferCache.findFromAddress(startAddress);
	if (color) {
		const float r = getBits<24, 8>(value) / 255.0f;
		const float g = getBits<16, 8>(value) / 255.0f;
		const float b = getBits<8, 8>(value) / 255.0f;
		const float a = (value & 0xff) / 255.0f;
		color->get().fbo.bind(OpenGL::DrawFramebuffer);

		gl.setColourMask(true, true, true, true);
		gl.setClearColour(r, g, b, a);
		OpenGL::clearColor();
		return;
	}

	const auto depth = depthBufferCache.findFromAddress(startAddress);
	if (depth) {
		depth->get().fbo.bind(OpenGL::DrawFramebuffer);

		float depthVal;
		const auto format = depth->get().format;
		if (format == DepthFmt::Depth16) {
			depthVal = (value & 0xffff) / 65535.0f;
		} else {
			depthVal = (value & 0xffffff) / 16777215.0f;
		}

		gl.setDepthMask(true);
		OpenGL::setClearDepth(depthVal);

		if (format == DepthFmt::Depth24Stencil8) {
			const u8 stencil = (value >> 24);
			gl.setStencilMask(0xff);
			OpenGL::setClearStencil(stencil);
			OpenGL::clearDepthAndStencil();
		} else {
			OpenGL::clearDepth();
		}

		return;
	}

	log("[RendererGL::ClearBuffer] No buffer found!\n");
}

OpenGL::Framebuffer RendererGL::getColourFBO() {
	// We construct a colour buffer object and see if our cache has any matching colour buffers in it
	// If not, we allocate a texture & FBO for our framebuffer and store it in the cache
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
		const u8* startPointer = gpu.getPointerPhys<u8>(tex.location);
		const usize sizeInBytes = tex.sizeInBytes();

		if (startPointer == nullptr || (sizeInBytes > 0 && gpu.getPointerPhys<u8>(tex.location + sizeInBytes - 1) == nullptr)) [[unlikely]] {
			Helpers::warn("Out-of-bounds texture fetch");
			return blankTexture;
		}

		const auto textureData = std::span{startPointer, tex.sizeInBytes()};  // Get pointer to the texture data in 3DS memory
		Texture& newTex = textureCache.add(tex);
		newTex.decodeTexture(textureData);

		return newTex.texture;
	}
}

// NOTE: The GPU format has RGB5551 and RGB655 swapped compared to internal regs format
PICA::ColorFmt ToColorFmt(u32 format) {
	switch (format) {
		case 2: return PICA::ColorFmt::RGB565;
		case 3: return PICA::ColorFmt::RGBA5551;
		default: return static_cast<PICA::ColorFmt>(format);
	}
}

void RendererGL::displayTransfer(u32 inputAddr, u32 outputAddr, u32 inputSize, u32 outputSize, u32 flags) {
	const u32 inputWidth = inputSize & 0xffff;
	const u32 inputHeight = inputSize >> 16;
	const auto inputFormat = ToColorFmt(Helpers::getBits<8, 3>(flags));
	const auto outputFormat = ToColorFmt(Helpers::getBits<12, 3>(flags));
	const bool verticalFlip = flags & 1;
	const PICA::Scaling scaling = static_cast<PICA::Scaling>(Helpers::getBits<24, 2>(flags));

	u32 outputWidth = outputSize & 0xffff;
	u32 outputHeight = outputSize >> 16;

	OpenGL::DebugScope scope("DisplayTransfer inputAddr 0x%08X outputAddr 0x%08X inputWidth %d outputWidth %d inputHeight %d outputHeight %d",
							 inputAddr, outputAddr, inputWidth, outputWidth, inputHeight, outputHeight);

	auto srcFramebuffer = getColourBuffer(inputAddr, inputFormat, inputWidth, outputHeight);
	Math::Rect<u32> srcRect = srcFramebuffer->getSubRect(inputAddr, outputWidth, outputHeight);

	if (verticalFlip) {
		std::swap(srcRect.bottom, srcRect.top);
	}

	// Apply scaling for the destination rectangle.
	if (scaling == PICA::Scaling::X || scaling == PICA::Scaling::XY) {
		outputWidth >>= 1;
	}

	if (scaling == PICA::Scaling::XY) {
		outputHeight >>= 1;
	}

	auto destFramebuffer = getColourBuffer(outputAddr, outputFormat, outputWidth, outputHeight);
	Math::Rect<u32> destRect = destFramebuffer->getSubRect(outputAddr, outputWidth, outputHeight);

	if (inputWidth != outputWidth) {
		// Helpers::warn("Strided display transfer is not handled correctly!\n");
	}

	// Blit the framebuffers
	srcFramebuffer->fbo.bind(OpenGL::ReadFramebuffer);
	destFramebuffer->fbo.bind(OpenGL::DrawFramebuffer);
	gl.disableScissor();

	glBlitFramebuffer(
		srcRect.left, srcRect.bottom, srcRect.right, srcRect.top, destRect.left, destRect.bottom, destRect.right, destRect.top, GL_COLOR_BUFFER_BIT,
		GL_LINEAR
	);
}

void RendererGL::textureCopy(u32 inputAddr, u32 outputAddr, u32 totalBytes, u32 inputSize, u32 outputSize, u32 flags) {
	// Texture copy size is aligned to 16 byte units
	const u32 copySize = totalBytes & ~0xf;
	if (copySize == 0) {
		printf("TextureCopy total bytes less than 16!\n");
		return;
	}

	// The width and gap are provided in 16-byte units.
	const u32 inputWidth = (inputSize & 0xffff) << 4;
	const u32 inputGap = (inputSize >> 16) << 4;
	const u32 outputWidth = (outputSize & 0xffff) << 4;
	const u32 outputGap = (outputSize >> 16) << 4;

	OpenGL::DebugScope scope("TextureCopy inputAddr 0x%08X outputAddr 0x%08X totalBytes %d inputWidth %d inputGap %d outputWidth %d outputGap %d",
							 inputAddr, outputAddr, totalBytes, inputWidth, inputGap, outputWidth, outputGap);

	if (inputGap != 0 || outputGap != 0) {
		// Helpers::warn("Strided texture copy\n");
	}

	if (inputWidth != outputWidth) {
		Helpers::warn("Input width does not match output width, cannot accelerate texture copy!");
		return;
	}

	// Texture copy is a raw data copy in PICA, which means no format or tiling information is provided to the engine.
	// Depending if the target surface is linear or tiled, games set inputWidth to either the width of the texture or
	// the width multiplied by eight (because tiles are stored linearly in memory).
	// To properly accelerate this we must examine each surface individually. For now we assume the most common case
	// of tiled surface with RGBA8 format. If our assumption does not hold true, we abort the texture copy as inserting
	// that surface is not correct.

	// We assume the source surface is tiled and RGBA8. inputWidth is in bytes so divide it
	// by eight * sizePerPixel(RGBA8) to convert it to a useable width.
	const u32 bpp = sizePerPixel(PICA::ColorFmt::RGBA8);
	const u32 copyStride = (inputWidth + inputGap) / (8 * bpp);
	const u32 copyWidth = inputWidth / (8 * bpp);

	// inputHeight/outputHeight are typically set to zero so they cannot be used to get the height of the copy region
	// in contrast to display transfer. Compute height manually by dividing the copy size with the copy width. The result
	// is the number of vertical tiles so multiply that by eight to get the actual copy height.
	u32 copyHeight;
	if (inputWidth != 0) [[likely]] {
		copyHeight = (copySize / inputWidth) * 8;
	} else {
		Helpers::warn("Zero-width texture copy");
		return;
	}

	// Find the source surface.
	auto srcFramebuffer = getColourBuffer(inputAddr, PICA::ColorFmt::RGBA8, copyStride, copyHeight, false);
	if (!srcFramebuffer) {
		static int shutUpCounter = 0; // Don't want to spam the console too much, so shut up after 5 times

		if (shutUpCounter < 5) {
			shutUpCounter++;
			printf("RendererGL::TextureCopy failed to locate src framebuffer!\n");
		}
		return;
	}

	Math::Rect<u32> srcRect = srcFramebuffer->getSubRect(inputAddr, copyWidth, copyHeight);

	// Assume the destination surface has the same format. Unless the surfaces have the same block width,
	// texture copy does not make sense.
	auto destFramebuffer = getColourBuffer(outputAddr, srcFramebuffer->format, copyWidth, copyHeight);
	Math::Rect<u32> destRect = destFramebuffer->getSubRect(outputAddr, copyWidth, copyHeight);

	// Blit the framebuffers
	srcFramebuffer->fbo.bind(OpenGL::ReadFramebuffer);
	destFramebuffer->fbo.bind(OpenGL::DrawFramebuffer);
	gl.disableScissor();

	glBlitFramebuffer(
		srcRect.left, srcRect.bottom, srcRect.right, srcRect.top, destRect.left, destRect.bottom, destRect.right, destRect.top, GL_COLOR_BUFFER_BIT,
		GL_LINEAR
	);
}

std::optional<ColourBuffer> RendererGL::getColourBuffer(u32 addr, PICA::ColorFmt format, u32 width, u32 height, bool createIfnotFound) {
	// Try to find an already existing buffer that contains the provided address
	// This is a more relaxed check compared to getColourFBO as display transfer/texcopy may refer to
	// subrect of a surface and in case of texcopy we don't know the format of the surface.
	auto buffer = colourBufferCache.findFromAddress(addr);
	if (buffer.has_value()) {
		return buffer.value().get();
	}

	if (!createIfnotFound) {
		return std::nullopt;
	}

	// Otherwise create and cache a new buffer.
	ColourBuffer sampleBuffer(addr, format, width, height);
	return colourBufferCache.add(sampleBuffer);
}

OpenGL::Program& RendererGL::getSpecializedShader() {
	constexpr uint vsUBOBlockBinding = 1;
	constexpr uint fsUBOBlockBinding = 2;

	PICA::FragmentConfig fsConfig(regs);
	// If we're not on GLES, ignore the logic op configuration and don't generate redundant shaders for it, since we use hw logic ops
	if (!driverInfo.usingGLES) {
		fsConfig.outConfig.logicOpMode = PICA::LogicOpMode(0);
	}

	OpenGL::Shader& fragShader = shaderCache.fragmentShaderCache[fsConfig];
	if (!fragShader.exists()) {
		std::string fs = fragShaderGen.generate(fsConfig);
		fragShader.create({fs.c_str(), fs.size()}, OpenGL::Fragment);
	}

	// Get the handle of the current vertex shader
	OpenGL::Shader& vertexShader = usingAcceleratedShader ? *generatedVertexShader : defaultShadergenVs;
	// And form the key for looking up a shader program
	const u64 programKey = (u64(vertexShader.handle()) << 32) | u64(fragShader.handle());

	CachedProgram& programEntry = shaderCache.programCache[programKey];
	OpenGL::Program& program = programEntry.program;

	if (!program.exists()) {
		program.create({vertexShader, fragShader});
		gl.useProgram(program);

		// Init sampler objects. Texture 0 goes in texture unit 0, texture 1 in TU 1, texture 2 in TU 2, and the light maps go in TU 3
		glUniform1i(OpenGL::uniformLocation(program, "u_tex0"), 0);
		glUniform1i(OpenGL::uniformLocation(program, "u_tex1"), 1);
		glUniform1i(OpenGL::uniformLocation(program, "u_tex2"), 2);
		glUniform1i(OpenGL::uniformLocation(program, "u_tex_luts"), 3);

		// Set up the binding for our UBOs. Sadly we can't specify it in the shader like normal people,
		// As it's an OpenGL 4.2 feature that MacOS doesn't support...
		uint fsUBOIndex = glGetUniformBlockIndex(program.handle(), "FragmentUniforms");
		glUniformBlockBinding(program.handle(), fsUBOIndex, fsUBOBlockBinding);

		if (usingAcceleratedShader) {
			uint vertexUBOIndex = glGetUniformBlockIndex(program.handle(), "PICAShaderUniforms");
			glUniformBlockBinding(program.handle(), vertexUBOIndex, vsUBOBlockBinding);
		}
	}
	glBindBufferBase(GL_UNIFORM_BUFFER, fsUBOBlockBinding, shadergenFragmentUBO);
	if (usingAcceleratedShader) {
		glBindBufferBase(GL_UNIFORM_BUFFER, vsUBOBlockBinding, hwShaderUniformUBO);
	}

	// Upload uniform data to our shader's UBO
	PICA::FragmentUniforms uniforms;
	uniforms.alphaReference = Helpers::getBits<8, 8>(regs[InternalRegs::AlphaTestConfig]);

	// Set up the texenv buffer color
	const u32 texEnvBufferColor = regs[InternalRegs::TexEnvBufferColor];
	uniforms.tevBufferColor[0] = float(texEnvBufferColor & 0xFF) / 255.0f;
	uniforms.tevBufferColor[1] = float((texEnvBufferColor >> 8) & 0xFF) / 255.0f;
	uniforms.tevBufferColor[2] = float((texEnvBufferColor >> 16) & 0xFF) / 255.0f;
	uniforms.tevBufferColor[3] = float((texEnvBufferColor >> 24) & 0xFF) / 255.0f;

	uniforms.depthScale = f24::fromRaw(regs[PICA::InternalRegs::DepthScale] & 0xffffff).toFloat32();
	uniforms.depthOffset = f24::fromRaw(regs[PICA::InternalRegs::DepthOffset] & 0xffffff).toFloat32();

	if (regs[InternalRegs::ClipEnable] & 1) {
		uniforms.clipCoords[0] = f24::fromRaw(regs[PICA::InternalRegs::ClipData0] & 0xffffff).toFloat32();
		uniforms.clipCoords[1] = f24::fromRaw(regs[PICA::InternalRegs::ClipData1] & 0xffffff).toFloat32();
		uniforms.clipCoords[2] = f24::fromRaw(regs[PICA::InternalRegs::ClipData2] & 0xffffff).toFloat32();
		uniforms.clipCoords[3] = f24::fromRaw(regs[PICA::InternalRegs::ClipData3] & 0xffffff).toFloat32();
	}

	// Set up the constant color for the 6 TEV stages
	for (int i = 0; i < 6; i++) {
		static constexpr std::array<u32, 6> ioBases = {
			PICA::InternalRegs::TexEnv0Source, PICA::InternalRegs::TexEnv1Source, PICA::InternalRegs::TexEnv2Source,
			PICA::InternalRegs::TexEnv3Source, PICA::InternalRegs::TexEnv4Source, PICA::InternalRegs::TexEnv5Source,
		};

		auto& vec = uniforms.constantColors[i];
		u32 base = ioBases[i];
		u32 color = regs[base + 3];

		vec[0] = float(color & 0xFF) / 255.0f;
		vec[1] = float((color >> 8) & 0xFF) / 255.0f;
		vec[2] = float((color >> 16) & 0xFF) / 255.0f;
		vec[3] = float((color >> 24) & 0xFF) / 255.0f;
	}

	uniforms.fogColor = regs[PICA::InternalRegs::FogColor];

	// Append lighting uniforms
	if (fsConfig.lighting.enable) {
		uniforms.globalAmbientLight = regs[InternalRegs::LightGlobalAmbient];
		for (int i = 0; i < 8; i++) {
			auto& light = uniforms.lightUniforms[i];
			const u32 specular0 = regs[InternalRegs::Light0Specular0 + i * 0x10];
			const u32 specular1 = regs[InternalRegs::Light0Specular1 + i * 0x10];
			const u32 diffuse = regs[InternalRegs::Light0Diffuse + i * 0x10];
			const u32 ambient = regs[InternalRegs::Light0Ambient + i * 0x10];
			const u32 lightXY = regs[InternalRegs::Light0XY + i * 0x10];
			const u32 lightZ = regs[InternalRegs::Light0Z + i * 0x10];

			const u32 spotlightXY = regs[InternalRegs::Light0SpotlightXY + i * 0x10];
			const u32 spotlightZ = regs[InternalRegs::Light0SpotlightZ + i * 0x10];
			const u32 attenuationBias = regs[InternalRegs::Light0AttenuationBias + i * 0x10];
			const u32 attenuationScale = regs[InternalRegs::Light0AttenuationScale + i * 0x10];

#define lightColorToVec3(value)                         \
	{                                                   \
		float(Helpers::getBits<20, 8>(value)) / 255.0f, \
		float(Helpers::getBits<10, 8>(value)) / 255.0f, \
		float(Helpers::getBits<0, 8>(value)) / 255.0f,  \
	}
			light.specular0 = lightColorToVec3(specular0);
			light.specular1 = lightColorToVec3(specular1);
			light.diffuse = lightColorToVec3(diffuse);
			light.ambient = lightColorToVec3(ambient);
			light.position[0] = Floats::f16::fromRaw(u16(lightXY)).toFloat32();
			light.position[1] = Floats::f16::fromRaw(u16(lightXY >> 16)).toFloat32();
			light.position[2] = Floats::f16::fromRaw(u16(lightZ)).toFloat32();

			// Fixed point 1.11.1 to float, without negation
			light.spotlightDirection[0] = float(s32(spotlightXY & 0x1FFF) << 19 >> 19) / 2047.0;
			light.spotlightDirection[1] = float(s32((spotlightXY >> 16) & 0x1FFF) << 19 >> 19) / 2047.0;
			light.spotlightDirection[2] = float(s32(spotlightZ & 0x1FFF) << 19 >> 19) / 2047.0;

			light.distanceAttenuationBias = Floats::f20::fromRaw(attenuationBias & 0xFFFFF).toFloat32();
			light.distanceAttenuationScale = Floats::f20::fromRaw(attenuationScale & 0xFFFFF).toFloat32();
#undef lightColorToVec3
		}
	}

	gl.bindUBO(shadergenFragmentUBO);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PICA::FragmentUniforms), &uniforms);

	return program;
}

bool RendererGL::prepareForDraw(ShaderUnit& shaderUnit, PICA::DrawAcceleration* accel) {
	// First we figure out if we will be using an ubershader
	bool usingUbershader = emulatorConfig->useUbershaders;
	if (usingUbershader) {
		const bool lightsEnabled = (regs[InternalRegs::LightingEnable] & 1) != 0;
		const uint lightCount = (regs[InternalRegs::LightNumber] & 0x7) + 1;

		// Emulating lights in the ubershader is incredibly slow, so we've got an option to render draws using moret han N lights via shadergen
		// This way we generate fewer shaders overall than with full shadergen, but don't tank performance
		if (emulatorConfig->forceShadergenForLights && lightsEnabled && lightCount >= emulatorConfig->lightShadergenThreshold) {
			usingUbershader = false;
		}
	}

	// Then we figure out if we will use hw accelerated shaders, and try to fetch our shader
	// TODO: Ubershader support for accelerated shaders
	usingAcceleratedShader = emulatorConfig->accelerateShaders && !usingUbershader && accel != nullptr && accel->canBeAccelerated;

	if (usingAcceleratedShader) {
		PICA::VertConfig vertexConfig(shaderUnit.vs, regs, usingUbershader);

		std::optional<OpenGL::Shader>& shader = shaderCache.vertexShaderCache[vertexConfig];
		// If the optional is false, we have never tried to recompile the shader before. Try to recompile it and see if it works.
		if (!shader.has_value()) {
			// Initialize shader to a "null" shader (handle == 0)
			shader = OpenGL::Shader();

			std::string picaShaderSource = PICA::ShaderGen::decompileShader(
				shaderUnit.vs, *emulatorConfig, shaderUnit.vs.entrypoint,
				driverInfo.usingGLES ? PICA::ShaderGen::API::GLES : PICA::ShaderGen::API::GL, PICA::ShaderGen::Language::GLSL
			);

			// Empty source means compilation error, if the source is not empty then we convert the recompiled PICA code into a valid shader and upload
			// it to the GPU
			if (!picaShaderSource.empty()) {
				std::string vertexShaderSource = fragShaderGen.getVertexShaderAccelerated(picaShaderSource, vertexConfig, usingUbershader);
				shader->create({vertexShaderSource}, OpenGL::Vertex);
			}
		}

		// Shader generation did not work out, so set usingAcceleratedShader to false
		if (!shader->exists()) {
			usingAcceleratedShader = false;
		} else {
			generatedVertexShader = &(*shader);
			gl.bindUBO(hwShaderUniformUBO);

			if (shaderUnit.vs.uniformsDirty) {
				shaderUnit.vs.uniformsDirty = false;
				glBufferSubData(GL_UNIFORM_BUFFER, 0, PICAShader::totalUniformSize(), shaderUnit.vs.getUniformPointer());
			}

			performIndexedRender = accel->indexed;
			minimumIndex = GLsizei(accel->minimumIndex);
			maximumIndex = GLsizei(accel->maximumIndex);

			// Upload vertex data and index buffer data to our GPU
			accelerateVertexUpload(shaderUnit, accel);
		}
	}

	if (!usingUbershader) {
		OpenGL::Program& program = getSpecializedShader();
		gl.useProgram(program);
	} else { // Bind ubershader & load ubershader uniforms
		gl.useProgram(triangleProgram);

		const float depthScale = f24::fromRaw(regs[PICA::InternalRegs::DepthScale] & 0xffffff).toFloat32();
		const float depthOffset = f24::fromRaw(regs[PICA::InternalRegs::DepthOffset] & 0xffffff).toFloat32();
		const bool depthMapEnable = regs[PICA::InternalRegs::DepthmapEnable] & 1;

		if (oldDepthScale != depthScale) {
			oldDepthScale = depthScale;
			glUniform1f(ubershaderData.depthScaleLoc, depthScale);
		}

		if (oldDepthOffset != depthOffset) {
			oldDepthOffset = depthOffset;
			glUniform1f(ubershaderData.depthOffsetLoc, depthOffset);
		}

		if (oldDepthmapEnable != depthMapEnable) {
			oldDepthmapEnable = depthMapEnable;
			glUniform1i(ubershaderData.depthmapEnableLoc, depthMapEnable);
		}

		// Upload PICA Registers as a single uniform. The shader needs access to the rasterizer registers (for depth, starting from index 0x48)
		// The texturing and the fragment lighting registers. Therefore we upload them all in one go to avoid multiple slow uniform updates
		glUniform1uiv(ubershaderData.picaRegLoc, 0x200 - 0x48, &regs[0x48]);
		setupUbershaderTexEnv();
	}

	return usingAcceleratedShader;
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
		std::memcpy(&flippedPixels[y * width * 4], &pixels[(height - y - 1) * width * 4], width * 4);
		// Swap R and B channels
		for (int x = 0; x < width; x++) {
			std::swap(flippedPixels[y * width * 4 + x * 4 + 0], flippedPixels[y * width * 4 + x * 4 + 2]);
			// Set alpha to 0xFF
			flippedPixels[y * width * 4 + x * 4 + 3] = 0xFF;
		}
	}

	stbi_write_png(name.c_str(), width, height, 4, flippedPixels.data(), 0);
}

void RendererGL::deinitGraphicsContext() {
	// Invalidate all surface caches since they'll no longer be valid
	textureCache.reset();
	depthBufferCache.reset();
	colourBufferCache.reset();
	shaderCache.clear();

	// All other GL objects should be invalidated automatically and be recreated by the next call to initGraphicsContext
	// TODO: Make it so that depth and colour buffers get written back to 3DS memory
	printf("RendererGL::DeinitGraphicsContext called\n");
}

std::string RendererGL::getUbershader() {
	auto gl_resources = cmrc::RendererGL::get_filesystem();
	auto fragmentShader = gl_resources.open("opengl_fragment_shader.frag");

	return std::string(fragmentShader.begin(), fragmentShader.end());
}

void RendererGL::setUbershader(const std::string& shader) {
	auto gl_resources = cmrc::RendererGL::get_filesystem();
	auto vertexShaderSource = gl_resources.open("opengl_vertex_shader.vert");

	OpenGL::Shader vert({vertexShaderSource.begin(), vertexShaderSource.size()}, OpenGL::Vertex);
	OpenGL::Shader frag(shader, OpenGL::Fragment);
	triangleProgram.create({vert, frag});

	initUbershader(triangleProgram);

	glUniform1f(ubershaderData.depthScaleLoc, oldDepthScale);
	glUniform1f(ubershaderData.depthOffsetLoc, oldDepthOffset);
	glUniform1i(ubershaderData.depthmapEnableLoc, oldDepthmapEnable);
}

void RendererGL::initUbershader(OpenGL::Program& program) {
	gl.useProgram(program);

	ubershaderData.textureEnvSourceLoc = OpenGL::uniformLocation(program, "u_textureEnvSource");
	ubershaderData.textureEnvOperandLoc = OpenGL::uniformLocation(program, "u_textureEnvOperand");
	ubershaderData.textureEnvCombinerLoc = OpenGL::uniformLocation(program, "u_textureEnvCombiner");
	ubershaderData.textureEnvColorLoc = OpenGL::uniformLocation(program, "u_textureEnvColor");
	ubershaderData.textureEnvScaleLoc = OpenGL::uniformLocation(program, "u_textureEnvScale");

	ubershaderData.depthScaleLoc = OpenGL::uniformLocation(program, "u_depthScale");
	ubershaderData.depthOffsetLoc = OpenGL::uniformLocation(program, "u_depthOffset");
	ubershaderData.depthmapEnableLoc = OpenGL::uniformLocation(program, "u_depthmapEnable");
	ubershaderData.picaRegLoc = OpenGL::uniformLocation(program, "u_picaRegs");

	// Init sampler objects. Texture 0 goes in texture unit 0, texture 1 in TU 1, texture 2 in TU 2 and the LUTs go in TU 3
	glUniform1i(OpenGL::uniformLocation(program, "u_tex0"), 0);
	glUniform1i(OpenGL::uniformLocation(program, "u_tex1"), 1);
	glUniform1i(OpenGL::uniformLocation(program, "u_tex2"), 2);
	glUniform1i(OpenGL::uniformLocation(program, "u_tex_luts"), 3);
}

void RendererGL::compileDisplayShader() {
	auto gl_resources = cmrc::RendererGL::get_filesystem();
	auto displayVertexShaderSource = driverInfo.usingGLES ? gl_resources.open("opengl_es_display.vert") : gl_resources.open("opengl_display.vert");
	auto displayFragmentShaderSource = driverInfo.usingGLES ? gl_resources.open("opengl_es_display.frag") : gl_resources.open("opengl_display.frag");

	OpenGL::Shader vertDisplay({displayVertexShaderSource.begin(), displayVertexShaderSource.size()}, OpenGL::Vertex);
	OpenGL::Shader fragDisplay({displayFragmentShaderSource.begin(), displayFragmentShaderSource.size()}, OpenGL::Fragment);
	displayProgram.create({vertDisplay, fragDisplay});

	gl.useProgram(displayProgram);
	glUniform1i(OpenGL::uniformLocation(displayProgram, "u_texture"), 0);  // Init sampler object
}

void RendererGL::accelerateVertexUpload(ShaderUnit& shaderUnit, PICA::DrawAcceleration* accel) {
	u32 buffer = 0;  // Vertex buffer index for non-fixed attributes
	u32 attrCount = 0;

	const u32 totalAttribCount = accel->totalAttribCount;

	static constexpr GLenum attributeFormats[4] = {
		GL_BYTE,           // 0: Signed byte
		GL_UNSIGNED_BYTE,  // 1: Unsigned byte
		GL_SHORT,          // 2: Short
		GL_FLOAT,          // 3: Float
	};

	const u32 vertexCount = accel->maximumIndex - accel->minimumIndex + 1;

	// Update index buffer if necessary
	if (accel->indexed) {
		usingShortIndices = accel->useShortIndices;
		const usize indexBufferSize = regs[PICA::InternalRegs::VertexCountReg] * (usingShortIndices ? sizeof(u16) : sizeof(u8));

		hwIndexBuffer->Bind();
		auto indexBufferRes = hwIndexBuffer->Map(4, indexBufferSize);
		hwIndexBufferOffset = reinterpret_cast<void*>(usize(indexBufferRes.buffer_offset));

		std::memcpy(indexBufferRes.pointer, accel->indexBuffer, indexBufferSize);
		// If we don't have glDrawRangeElementsBaseVertex, we must subtract the base index value from our index buffer manually
		if (glDrawRangeElementsBaseVertex == nullptr) [[unlikely]] {
			const u32 indexCount = regs[PICA::InternalRegs::VertexCountReg];
			usingShortIndices ? PICA::IndexBuffer::subtractBaseIndex<true>((u8*)indexBufferRes.pointer, indexCount, accel->minimumIndex)
							  : PICA::IndexBuffer::subtractBaseIndex<false>((u8*)indexBufferRes.pointer, indexCount, accel->minimumIndex);
		}

		hwIndexBuffer->Unmap(indexBufferSize);
	}

	hwVertexBuffer->Bind();
	auto vertexBufferRes = hwVertexBuffer->Map(4, accel->vertexDataSize);
	u8* vertexData = static_cast<u8*>(vertexBufferRes.pointer);
	const u32 vertexBufferOffset = vertexBufferRes.buffer_offset;

	gl.bindVAO(hwShaderVAO);

	// Enable or disable vertex attributes as needed
	const u32 currentAttributeMask = accel->enabledAttributeMask;
	// Use bitwise xor to calculate which attributes changed
	u32 attributeMaskDiff = currentAttributeMask ^ previousAttributeMask;
	
	while (attributeMaskDiff != 0) {
		// Get index of next different attribute and turn it off
		const u32 index = 31 - std::countl_zero<u32>(attributeMaskDiff);
		const u32 mask = 1u << index;
		attributeMaskDiff ^= mask;

		if ((currentAttributeMask & mask) != 0) {
			// Attribute was disabled and is now enabled
			hwShaderVAO.enableAttribute(index);
		} else {
			// Attribute was enabled and is now disabled
			hwShaderVAO.disableAttribute(index);
		}
	}

	previousAttributeMask = currentAttributeMask;

	// Upload the data for each (enabled) attribute loader into our vertex buffer
	for (int i = 0; i < accel->totalLoaderCount; i++) {
		auto& loader = accel->loaders[i];

		std::memcpy(vertexData, loader.data, loader.size);
		vertexData += loader.size;
	}

	hwVertexBuffer->Unmap(accel->vertexDataSize);

	// Iterate over the 16 PICA input registers and configure how they should be fetched.
	for (int i = 0; i < 16; i++) {
		const auto& attrib = accel->attributeInfo[i];
		const u32 attributeMask = 1u << i;

		if (accel->fixedAttributes & attributeMask) {
			auto& attrValue = fixedAttrValues[i];
			// This is a fixed attribute, so set its fixed value, but only if it actually needs to be updated
			if (attrValue[0] != attrib.fixedValue[0] || attrValue[1] != attrib.fixedValue[1] || attrValue[2] != attrib.fixedValue[2] ||
				attrValue[3] != attrib.fixedValue[3]) {
				std::memcpy(attrValue.data(), attrib.fixedValue.data(), sizeof(attrib.fixedValue));
				glVertexAttrib4f(i, attrib.fixedValue[0], attrib.fixedValue[1], attrib.fixedValue[2], attrib.fixedValue[3]);
			}
		} else if (accel->enabledAttributeMask & attributeMask) {
			glVertexAttribPointer(
				i, attrib.componentCount, attributeFormats[attrib.type], GL_FALSE, attrib.stride,
				reinterpret_cast<GLvoid*>(vertexBufferOffset + attrib.offset)
			);
		}
	}
}

void RendererGL::setupGLES() {
	driverInfo.usingGLES = true;

	// OpenGL ES hardware is typically way too slow to use the ubershader (eg RPi, mobile phones, handhelds) or has other issues with it.
	// So, display a warning and turn them off on OpenGL ES.
	if (emulatorConfig->useUbershaders) {
		emulatorConfig->useUbershaders = false;
		Helpers::warn("Ubershaders enabled on OpenGL ES. This usually results in a worse experience, turning it off...");
	}

	// Stub out logic operations so that calling them doesn't crash the emulator
	glLogicOp = [](GLenum) {};
}