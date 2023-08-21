#include "renderer_gl/gl_state.hpp"

void GLStateManager::resetBlend() {
	blendEnabled = false;
	logicOpEnabled = false;
	logicOp = GL_COPY;

	OpenGL::disableBlend();
	OpenGL::disableLogicOp();
	OpenGL::setLogicOp(GL_COPY);
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

void GLStateManager::resetVBO() {
	boundVBO = 0;
	glBindBuffer(GL_ARRAY_BUFFER, 0);
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
	resetVBO();
	resetProgram();
	resetScissor();
	resetStencil();
}