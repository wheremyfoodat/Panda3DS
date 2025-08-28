#pragma once
#include "opengl.hpp"

// Information about our OpenGL/OpenGL ES driver that we should keep track of
// Stuff like whether specific extensions are supported, and potentially things like OpenGL context information
namespace OpenGL {
	struct Driver {
		bool usingGLES = false;
		bool supportsExtFbFetch = false;
		bool supportsArmFbFetch = false;

		// Minimum alignment for UBO offsets. Fetched by the OpenGL renderer using glGetIntegerV.
		GLuint uboAlignment = 16;

		bool supportFbFetch() const { return supportsExtFbFetch || supportsArmFbFetch; }
	};
}  // namespace OpenGL