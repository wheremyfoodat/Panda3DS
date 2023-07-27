#include "renderer_gl/gl_state.hpp"

void GLStateManager::resetBlend() {
	blendEnabled = false;
	logicOpEnabled = false;

	OpenGL::disableBlend();
	OpenGL::disableLogicOp();
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
	OpenGL::disableStencil();
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
	resetClipping();
	resetColourMask();
	resetDepth();

	resetVAO();
	resetVBO();
	resetProgram();
	resetScissor();
	resetStencil();
}