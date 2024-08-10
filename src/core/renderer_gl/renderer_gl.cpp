#include "renderer_gl/renderer_gl.hpp"

#include <stb_image_write.h>

#include <cmrc/cmrc.hpp>

#include "config.hpp"
#include "PICA/float_types.hpp"
#include "PICA/pica_frag_uniforms.hpp"
#include "PICA/gpu.hpp"
#include "PICA/regs.hpp"
#include "renderer_gl/async_compiler.hpp"
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

	clearShaderCache();

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

#ifdef __ANDROID__
	fragShaderGen.setTarget(PICA::ShaderGen::API::GLES, PICA::ShaderGen::Language::GLSL);
#endif
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

	auto displayVertexShaderSource = gl_resources.open("opengl_display.vert");
	auto displayFragmentShaderSource = gl_resources.open("opengl_display.frag");

	OpenGL::Shader vertDisplay({displayVertexShaderSource.begin(), displayVertexShaderSource.size()}, OpenGL::Vertex);
	OpenGL::Shader fragDisplay({displayFragmentShaderSource.begin(), displayFragmentShaderSource.size()}, OpenGL::Fragment);
	displayProgram.create({vertDisplay, fragDisplay});

	gl.useProgram(displayProgram);
	glUniform1i(OpenGL::uniformLocation(displayProgram, "u_texture"), 0);  // Init sampler object

	// Allocate memory for the shadergen fragment uniform UBO
	glGenBuffers(1, &shadergenFragmentUBO);
	gl.bindUBO(shadergenFragmentUBO);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(PICA::FragmentUniforms), nullptr, GL_DYNAMIC_DRAW);

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

	reset();

	// Initialize the default vertex shader used with shadergen
	std::string defaultShadergenVSSource = fragShaderGen.getDefaultVertexShader();
	defaultShadergenVs.create({defaultShadergenVSSource.c_str(), defaultShadergenVSSource.size()}, OpenGL::Vertex);
}

void RendererGL::initGraphicsContext(SDL_Window* context) {
	if (shaderMode == ShaderMode::Hybrid) {
		asyncCompiler = new AsyncCompilerThread(fragShaderGen, context);
	}

	initGraphicsContextInternal();
}

#ifdef PANDA3DS_FRONTEND_QT
void RendererGL::initGraphicsContext(GL::Context* context) {
	if (shaderMode == ShaderMode::Hybrid) {
		asyncCompiler = new AsyncCompilerThread(fragShaderGen, context);
	}

	initGraphicsContextInternal();
}
#endif

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

	if (shaderMode == ShaderMode::Ubershader) {
		const bool lightsEnabled = (regs[InternalRegs::LightingEnable] & 1) != 0;
		const uint lightCount = (regs[InternalRegs::LightNumber] & 0x7) + 1;

		// Emulating lights in the ubershader is incredibly slow, so we've got an option to render draws using moret han N lights via shadergen
		// This way we generate fewer shaders overall than with full shadergen, but don't tank performance 
		if (emulatorConfig->forceShadergenForLights && lightsEnabled && lightCount >= emulatorConfig->lightShadergenThreshold) {
			OpenGL::Program& program = getSpecializedShader();
			gl.useProgram(program);
		} else {
			gl.useProgram(triangleProgram);
		}
	} else if (shaderMode == ShaderMode::Specialized) {
		OpenGL::Program& program = getSpecializedShader();
		gl.useProgram(program);
	} else if (shaderMode == ShaderMode::Hybrid) {
		PICA::FragmentConfig fsConfig(regs);
		auto cachedProgram = shaderCache.find(fsConfig);

		if (cachedProgram == shaderCache.end()) {
			CachedProgram& program = shaderCache[fsConfig];
			program.compiling.store(true);
			asyncCompiler->PushFragmentConfig(fsConfig, &program);
			gl.useProgram(triangleProgram);
		} else if (cachedProgram->second.compiling.load(std::memory_order_relaxed)) {
			gl.useProgram(triangleProgram);
		} else {
			OpenGL::Program& program = getSpecializedShader();
			gl.useProgram(program);
		}
	} else {
		Helpers::panic("Invalid shader mode");
	}

	const auto primitiveTopology = primTypes[static_cast<usize>(primType)];
	gl.disableScissor();
	gl.bindVBO(vbo);
	gl.bindVAO(vao);

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

	// Update ubershader uniforms
	if (gl.currentProgram == triangleProgram.handle()) {
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

	vbo.bufferVertsSub(vertices);
	OpenGL::draw(primitiveTopology, GLsizei(vertices.size()));
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
	constexpr uint uboBlockBinding = 2;

	PICA::FragmentConfig fsConfig(regs);

	CachedProgram& programEntry = shaderCache[fsConfig];
	OpenGL::Program& program = programEntry.program;

	if (!program.exists()) {
		if (shaderMode == ShaderMode::Hybrid) [[unlikely]] {
			Helpers::panic("Compiling shaders in main thread, this should never happen");
		}

		std::string fs = fragShaderGen.generate(fsConfig);

		OpenGL::Shader fragShader({fs.c_str(), fs.size()}, OpenGL::Fragment);
		program.create({defaultShadergenVs, fragShader});

		fragShader.free();
	}

	if (programEntry.needsInitialization) {
		gl.useProgram(program);
		// Init sampler objects. Texture 0 goes in texture unit 0, texture 1 in TU 1, texture 2 in TU 2, and the light maps go in TU 3
		glUniform1i(OpenGL::uniformLocation(program, "u_tex0"), 0);
		glUniform1i(OpenGL::uniformLocation(program, "u_tex1"), 1);
		glUniform1i(OpenGL::uniformLocation(program, "u_tex2"), 2);
		glUniform1i(OpenGL::uniformLocation(program, "u_tex_luts"), 3);

		// Set up the binding for our UBO. Sadly we can't specify it in the shader like normal people,
		// As it's an OpenGL 4.2 feature that MacOS doesn't support...
		uint uboIndex = glGetUniformBlockIndex(program.handle(), "FragmentUniforms");
		glUniformBlockBinding(program.handle(), uboIndex, uboBlockBinding);
		programEntry.needsInitialization = false;
	}
	glBindBufferBase(GL_UNIFORM_BUFFER, uboBlockBinding, shadergenFragmentUBO);

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

void RendererGL::clearShaderCache() {
	if (asyncCompiler != nullptr && shaderMode == ShaderMode::Hybrid) {
		// May contain objects that are still in use, so we need to clear them first
		asyncCompiler->Finish();
	}

	for (auto& shader : shaderCache) {
		CachedProgram& cachedProgram = shader.second;
		cachedProgram.program.free();
	}

	shaderCache.clear();
}

void RendererGL::deinitGraphicsContext() {
	// Invalidate all surface caches since they'll no longer be valid
	textureCache.reset();
	depthBufferCache.reset();
	colourBufferCache.reset();
	clearShaderCache();

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
