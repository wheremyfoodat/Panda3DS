#include "gl_state.hpp"

void GLStateManager::resetBlend() {
	blendEnabled = false;
	OpenGL::disableBlend();
}

void GLStateManager::resetDepth() {
	depthEnabled = false;
	OpenGL::disableDepth();
}

void GLStateManager::reset() {
	resetBlend();
	resetDepth();
}