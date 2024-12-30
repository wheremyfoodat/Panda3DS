#pragma once

// Information about our OpenGL/OpenGL ES driver that we should keep track of
// Stuff like whether specific extensions are supported, and potentially things like OpenGL context information
namespace OpenGL {
	struct Driver {
		bool usingGLES = false;
		bool supportsExtFbFetch = false;
		bool supportsArmFbFetch = false;

		bool supportFbFetch() const { return supportsExtFbFetch || supportsArmFbFetch; }
	};
}  // namespace OpenGL