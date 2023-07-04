#include "gl_state.hpp"

void GLStateManager::resetBlend() {
	blendEnabled = false;
	OpenGL::disableBlend();
}

void GLStateManager::resetDepth() {
	depthEnabled = false;
	OpenGL::disableDepth();
}

void GLStateManager::resetScissor() {
	scissorEnabled = false;
	OpenGL::disableScissor();
	OpenGL::setScissor(0, 0, 0, 0);
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
	resetDepth();

	resetVAO();
	resetVBO();
	resetProgram();
	resetScissor();
}