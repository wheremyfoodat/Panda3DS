#include "renderer_gl/gl_state.hpp"

void GLStateManager::resetBlend() {
	blendEnabled = false;
	logicOpEnabled = false;
	logicOp = GL_COPY;

	blendEquationRGB = GL_FUNC_ADD;
	blendEquationAlpha = GL_FUNC_ADD;

	blendFuncSourceRGB = GL_SRC_COLOR;
	blendFuncDestRGB = GL_DST_COLOR;
	blendFuncSourceAlpha = GL_SRC_ALPHA;
	blendFuncDestAlpha = GL_DST_ALPHA;

	OpenGL::disableBlend();
	OpenGL::disableLogicOp();
	OpenGL::setLogicOp(GL_COPY);

	glBlendEquationSeparate(blendEquationRGB, blendEquationAlpha);
	glBlendFuncSeparate(blendFuncSourceRGB, blendFuncDestRGB, blendFuncSourceAlpha, blendFuncDestAlpha);
}

void GLStateManager::resetClearing() {
	clearRed = 0.f;
	clearBlue = 0.f;
	clearGreen = 0.f;
	clearAlpha = 1.f;

	OpenGL::setClearColor(clearRed, clearBlue, clearGreen, clearAlpha);
}

void GLStateManager::resetClipping() {
	// Disable all (supported) clip planes
	enabledClipPlanes = 0;
	for (int i = 0; i < clipPlaneCount; i++) {
		OpenGL::disableClipPlane(i);
	}
}

void GLStateManager::resetColourMask() {
	redMask = greenMask = blueMask = alphaMask = true;
	OpenGL::setColourMask(redMask, greenMask, blueMask, alphaMask);
}

void GLStateManager::resetDepth() {
	depthEnabled = false;
	depthMask = true;
	depthFunc = GL_LESS;

	OpenGL::disableDepth();
	OpenGL::setDepthMask(true);
	OpenGL::setDepthFunc(OpenGL::DepthFunc::Less);
}

void GLStateManager::resetScissor() {
	scissorEnabled = false;
	OpenGL::disableScissor();
	OpenGL::setScissor(0, 0, 0, 0);
}

void GLStateManager::resetStencil() {
	stencilEnabled = false;
	stencilMask = 0xff;

	OpenGL::disableStencil();
	OpenGL::setStencilMask(0xff);
}

void GLStateManager::resetVAO() {
	boundVAO = 0;
	glBindVertexArray(0);
}

void GLStateManager::resetBuffers() {
	boundUBO = 0;
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void GLStateManager::resetProgram() {
	currentProgram = 0;
	glUseProgram(0);
}

void GLStateManager::reset() {
	resetBlend();
	resetClearing();
	resetClipping();
	resetColourMask();
	resetDepth();

	resetVAO();
	resetBuffers();
	resetProgram();
	resetScissor();
	resetStencil();
}