#pragma once

#include <array>
#include <cstring>
#include <functional>
#include <span>
#include <unordered_map>

#include "PICA/float_types.hpp"
#include "PICA/pica_frag_config.hpp"
#include "PICA/pica_hash.hpp"
#include "PICA/pica_vertex.hpp"
#include "PICA/regs.hpp"
#include "PICA/shader_gen.hpp"
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
	bool enableUbershader = true;

	// Data 
	struct {
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
	} ubershaderData;

	float oldDepthScale = -1.0;
	float oldDepthOffset = 0.0;
	bool oldDepthmapEnable = false;

	SurfaceCache<DepthBuffer, 16, true> depthBufferCache;
	SurfaceCache<ColourBuffer, 16, true> colourBufferCache;
	SurfaceCache<Texture, 256, true> textureCache;

	// Dummy VAO/VBO for blitting the final output
	OpenGL::VertexArray dummyVAO;
	OpenGL::VertexBuffer dummyVBO;

	OpenGL::Texture screenTexture;
	OpenGL::Texture LUTTexture;
	OpenGL::Framebuffer screenFramebuffer;
	OpenGL::Texture blankTexture;
	// The "default" vertex shader to use when using specialized shaders but not PICA vertex shader -> GLSL recompilation
	// We can compile this once and then link it with all other generated fragment shaders
	OpenGL::Shader defaultShadergenVs;

	// Cached recompiled fragment shader
	struct CachedProgram {
		OpenGL::Program program;
		uint uboBinding;
	};
	std::unordered_map<PICA::FragmentConfig, CachedProgram> shaderCache;

	OpenGL::Framebuffer getColourFBO();
	OpenGL::Texture getTexture(Texture& tex);
	OpenGL::Program& getSpecializedShader();

	PICA::ShaderGen::FragmentGenerator fragShaderGen;

	MAKE_LOG_FUNCTION(log, rendererLogger)
	void setupBlending();
	void setupStencilTest(bool stencilEnable);
	void bindDepthBuffer();
	void setupUbershaderTexEnv();
	void bindTexturesToSlots();
	void updateLightingLUT();
	void updateFogLUT();
	void initGraphicsContextInternal();

  public:
	RendererGL(GPU& gpu, const std::array<u32, regNum>& internalRegs, const std::array<u32, extRegNum>& externalRegs)
		: Renderer(gpu, internalRegs, externalRegs), fragShaderGen(PICA::ShaderGen::API::GL, PICA::ShaderGen::Language::GLSL) {}
	~RendererGL() override;

	void reset() override;
	void display() override;                                                              // Display the 3DS screen contents to the window
	void initGraphicsContext(SDL_Window* window) override;                                // Initialize graphics context
	void clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) override;  // Clear a GPU buffer in VRAM
	void displayTransfer(u32 inputAddr, u32 outputAddr, u32 inputSize, u32 outputSize, u32 flags) override;  // Perform display transfer
	void textureCopy(u32 inputAddr, u32 outputAddr, u32 totalBytes, u32 inputSize, u32 outputSize, u32 flags) override;
	void drawVertices(PICA::PrimType primType, std::span<const PICA::Vertex> vertices) override;             // Draw the given vertices
	void deinitGraphicsContext() override;

	virtual bool supportsShaderReload() override { return true; }
	virtual std::string getUbershader() override;
	virtual void setUbershader(const std::string& shader) override;

	virtual void setUbershaderSetting(bool value) override { enableUbershader = value; }
	
	std::optional<ColourBuffer> getColourBuffer(u32 addr, PICA::ColorFmt format, u32 width, u32 height, bool createIfnotFound = true);

	// Note: The caller is responsible for deleting the currently bound FBO before calling this
	void setFBO(uint handle) { screenFramebuffer.m_handle = handle; }
	void resetStateManager() { gl.reset(); }
	void clearShaderCache();
	void initUbershader(OpenGL::Program& program);

#ifdef PANDA3DS_FRONTEND_QT
	virtual void initGraphicsContext([[maybe_unused]] GL::Context* context) override { initGraphicsContextInternal(); }
#endif

	// Take a screenshot of the screen and store it in a file
	void screenshot(const std::string& name) override;
};