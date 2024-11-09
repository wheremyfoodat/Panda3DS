#pragma once
#include <type_traits>

#include "helpers.hpp"
#include "opengl.hpp"

// GL state manager object for use in the OpenGL GPU renderer and potentially other things in the future (such as a potential ImGui GUI)
// This object is meant to help us avoid duplicate OpenGL calls (such as binding the same program twice, enabling/disabling a setting twice, etc)
// by checking if we actually *need* a state change. This is meant to avoid expensive driver calls and minimize unneeded state changes
// A lot of code is in the header file instead of the relevant source file to make sure stuff gets inlined even without LTO, and
// because this header should ideally not be getting included in too many places
// Code that does not need inlining however, like the reset() function should be in gl_state.cpp
// This state manager may not handle every aspect of OpenGL, in which case anything not handled here should just be manipulated with raw
// OpenGL/opengl.hpp calls However, anything that can be handled through the state manager should, or at least there should be an attempt to keep it
// consistent with the current GL state to avoid bugs/suboptimal code.

// The state manager must *also* be a trivially constructible/destructible type, to ensure that no OpenGL functions get called sneakily without us
// knowing. This is important for when we want to eg add a Vulkan or misc backend. Would definitely not want to refactor all this. So we try to be as
// backend-agnostic as possible

struct GLStateManager {
	// We only support 6 clipping planes in our state manager because that's the minimum for GL_MAX_CLIP_PLANES
	// And nobody needs more than 6 clip planes anyways
	static constexpr GLint clipPlaneCount = 6;

	bool blendEnabled;
	bool logicOpEnabled;
	bool depthEnabled;
	bool scissorEnabled;
	bool stencilEnabled;
	u32 enabledClipPlanes;  // Bitfield of enabled clip planes

	// Colour/depth masks
	bool redMask, greenMask, blueMask, alphaMask;
	bool depthMask;

	float clearRed, clearBlue, clearGreen, clearAlpha;
	
	GLuint stencilMask;
	GLuint boundVAO;
	GLuint currentProgram;
	GLuint boundUBO;

	GLenum depthFunc;
	GLenum logicOp;
	GLenum blendEquationRGB, blendEquationAlpha;
	GLenum blendFuncSourceRGB, blendFuncSourceAlpha;
	GLenum blendFuncDestRGB, blendFuncDestAlpha;

	void reset();
	void resetBlend();
	void resetClearing();
	void resetClipping();
	void resetColourMask();
	void resetDepth();
	void resetVAO();
	void resetBuffers();
	void resetProgram();
	void resetScissor();
	void resetStencil();

	void enableDepth() {
		if (!depthEnabled) {
			depthEnabled = true;
			OpenGL::enableDepth();
		}
	}

	void disableDepth() {
		if (depthEnabled) {
			depthEnabled = false;
			OpenGL::disableDepth();
		}
	}

	void enableBlend() {
		if (!blendEnabled) {
			blendEnabled = true;
			OpenGL::enableBlend();
		}
	}

	void disableBlend() {
		if (blendEnabled) {
			blendEnabled = false;
			OpenGL::disableBlend();
		}
	}

	void enableScissor() {
		if (!scissorEnabled) {
			scissorEnabled = true;
			OpenGL::enableScissor();
		}
	}

	void disableScissor() {
		if (scissorEnabled) {
			scissorEnabled = false;
			OpenGL::disableScissor();
		}
	}

	void enableStencil() {
		if (!stencilEnabled) {
			stencilEnabled = true;
			OpenGL::enableStencil();
		}
	}

	void disableStencil() {
		if (stencilEnabled) {
			stencilEnabled = false;
			OpenGL::disableStencil();
		}
	}

	void enableLogicOp() {
		if (!logicOpEnabled) {
			logicOpEnabled = true;
			OpenGL::enableLogicOp();
		}
	}

	void disableLogicOp() {
		if (logicOpEnabled) {
			logicOpEnabled = false;
			OpenGL::disableLogicOp();
		}
	}

	void setLogicOp(GLenum op) {
		if (logicOp != op) {
			logicOp = op;
			OpenGL::setLogicOp(op);
		}
	}

	void enableClipPlane(GLuint index) {
		if (index >= clipPlaneCount) [[unlikely]] {
			Helpers::panic("Enabled invalid clipping plane %d\n", index);
		}

		if ((enabledClipPlanes & (1 << index)) == 0) {
			enabledClipPlanes |= 1 << index;  // Enable relevant bit in clipping plane bitfield
			OpenGL::enableClipPlane(index);   // Enable plane
		}
	}

	void disableClipPlane(GLuint index) {
		if (index >= clipPlaneCount) [[unlikely]] {
			Helpers::panic("Disabled invalid clipping plane %d\n", index);
		}

		if ((enabledClipPlanes & (1 << index)) != 0) {
			enabledClipPlanes ^= 1 << index;  // Disable relevant bit in bitfield by flipping it
			OpenGL::disableClipPlane(index);  // Disable plane
		}
	}

	void setStencilMask(GLuint mask) {
		if (stencilMask != mask) {
			stencilMask = mask;
			OpenGL::setStencilMask(mask);
		}
	}

	void bindVAO(GLuint handle) {
		if (boundVAO != handle) {
			boundVAO = handle;
			glBindVertexArray(handle);
		}
	}

	void useProgram(GLuint handle) {
		if (currentProgram != handle) {
			currentProgram = handle;
			glUseProgram(handle);
		}
	}

	void bindUBO(GLuint handle) {
		if (boundUBO != handle) {
			boundUBO = handle;
			glBindBuffer(GL_UNIFORM_BUFFER, boundUBO);
		}
	}

	void bindVAO(const OpenGL::VertexArray& vao) { bindVAO(vao.handle()); }
	void useProgram(const OpenGL::Program& program) { useProgram(program.handle()); }

	void setColourMask(bool r, bool g, bool b, bool a) {
		if (r != redMask || g != greenMask || b != blueMask || a != alphaMask) {
			r = redMask;
			g = greenMask;
			b = blueMask;
			a = alphaMask;

			OpenGL::setColourMask(r, g, b, a);
		}
	}

	void setDepthMask(bool mask) {
		if (depthMask != mask) {
			depthMask = mask;
			OpenGL::setDepthMask(mask);
		}
	}

	void setDepthFunc(GLenum func) {
		if (depthFunc != func) {
			depthFunc = func;
			glDepthFunc(func);
		}
	}

	void setClearColour(float r, float g, float b, float a) {
		if (clearRed != r || clearGreen != g || clearBlue != b || clearAlpha != a) {
			clearRed = r;
			clearGreen = g;
			clearBlue = b;
			clearAlpha = a;

			OpenGL::setClearColor(r, g, b, a);
		}
	}

	void setDepthFunc(OpenGL::DepthFunc func) { setDepthFunc(static_cast<GLenum>(func)); }

	// Counterpart to glBlendEquationSeparate
	void setBlendEquation(GLenum modeRGB, GLenum modeAlpha) {
		if (blendEquationRGB != modeRGB || blendEquationAlpha != modeAlpha) {
			blendEquationRGB = modeRGB;
			blendEquationAlpha = modeAlpha;

			glBlendEquationSeparate(modeRGB, modeAlpha);
		}
	}

	// Counterpart to glBlendFuncSeparate
	void setBlendFunc(GLenum sourceRGB, GLenum destRGB, GLenum sourceAlpha, GLenum destAlpha) {
		if (blendFuncSourceRGB != sourceRGB || blendFuncDestRGB != destRGB || blendFuncSourceAlpha != sourceAlpha ||
			blendFuncDestAlpha != destAlpha) {

			blendFuncSourceRGB = sourceRGB;
			blendFuncDestRGB = destRGB;
			blendFuncSourceAlpha = sourceAlpha;
			blendFuncDestAlpha = destAlpha;

			glBlendFuncSeparate(sourceRGB, destRGB,sourceAlpha, destAlpha);
		}
	}

	// Counterpart to regular glBlendEquation
	void setBlendEquation(GLenum mode) { setBlendEquation(mode, mode); }

	void setBlendEquation(OpenGL::BlendEquation modeRGB, OpenGL::BlendEquation modeAlpha) {
		setBlendEquation(static_cast<GLenum>(modeRGB), static_cast<GLenum>(modeAlpha));
	}

	void setBlendEquation(OpenGL::BlendEquation mode) {
		setBlendEquation(static_cast<GLenum>(mode));
	}
};

static_assert(std::is_trivially_constructible<GLStateManager>(), "OpenGL State Manager class is not trivially constructible!");
static_assert(std::is_trivially_destructible<GLStateManager>(), "OpenGL State Manager class is not trivially destructible!");