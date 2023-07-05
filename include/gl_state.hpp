#pragma once
#include <type_traits>

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
	bool blendEnabled;
	bool depthEnabled;
	bool scissorEnabled;

	// Colour mask
	bool redMask, greenMask, blueMask, alphaMask;

	GLuint boundVAO;
	GLuint boundVBO;
	GLuint currentProgram;

	void reset();
	void resetBlend();
	void resetColourMask();
	void resetDepth();
	void resetVAO();
	void resetVBO();
	void resetProgram();
	void resetScissor();

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

	void bindVAO(GLuint handle) {
		if (boundVAO != handle) {
			boundVAO = handle;
			glBindVertexArray(handle);
		}
	}

	void bindVBO(GLuint handle) {
		if (boundVBO != handle) {
			boundVBO = handle;
			glBindBuffer(GL_ARRAY_BUFFER, handle);
		}
	}

	void useProgram(GLuint handle) {
		if (currentProgram != handle) {
			currentProgram = handle;
			glUseProgram(handle);
		}
	}

	void setColourMask(bool r, bool g, bool b, bool a) {
		if (r != redMask || g != greenMask || b != blueMask || a != alphaMask) {
			r = redMask;
			g = greenMask;
			b = blueMask;
			a = alphaMask;

			glColorMask(r, g, b, a);
		}
	}

	void bindVAO(const OpenGL::VertexArray& vao) { bindVAO(vao.handle()); }
	void bindVBO(const OpenGL::VertexBuffer& vbo) { bindVBO(vbo.handle()); }
	void useProgram(const OpenGL::Program& program) { useProgram(program.handle()); }
};

static_assert(std::is_trivially_constructible<GLStateManager>(), "OpenGL State Manager class is not trivially constructible!");
static_assert(std::is_trivially_destructible<GLStateManager>(), "OpenGL State Manager class is not trivially destructible!");