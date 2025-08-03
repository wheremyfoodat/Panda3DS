#include "panda_qt/screen/screen_gl.hpp"

#include <array>

#ifdef PANDA3DS_ENABLE_OPENGL
ScreenWidgetGL::ScreenWidgetGL(API api, ResizeCallback resizeCallback, QWidget* parent) : ScreenWidget(api, resizeCallback, parent) {
	// On Wayland + OpenGL, we have to show the window before we can create a graphics context.
	resize(800, 240 * 4);
	show();

	if (!createContext()) {
		Helpers::panic("Failed to create GL context for display");
	}
}

bool ScreenWidgetGL::createContext() {
	// List of GL context versions we will try. Anything 4.1+ is good for desktop OpenGL, and 3.1+ for OpenGL ES
	static constexpr std::array<GL::Context::Version, 8> versionsToTry = {
		GL::Context::Version{GL::Context::Profile::Core, 4, 6}, GL::Context::Version{GL::Context::Profile::Core, 4, 5},
		GL::Context::Version{GL::Context::Profile::Core, 4, 4}, GL::Context::Version{GL::Context::Profile::Core, 4, 3},
		GL::Context::Version{GL::Context::Profile::Core, 4, 2}, GL::Context::Version{GL::Context::Profile::Core, 4, 1},
		GL::Context::Version{GL::Context::Profile::ES, 3, 2},   GL::Context::Version{GL::Context::Profile::ES, 3, 1},
	};

	std::optional<WindowInfo> windowInfo = getWindowInfo();
	if (windowInfo.has_value()) {
		this->windowInfo = *windowInfo;

		glContext = GL::Context::Create(*getWindowInfo(), versionsToTry);
		if (glContext == nullptr) {
			return false;
		}

		glContext->DoneCurrent();
	}

	return glContext != nullptr;
}

void ScreenWidgetGL::resizeDisplay() {
	// This will call take care of calling resizeSurface from the emulator thread, as the GL renderer must resize from the emu thread
	resizeCallback(surfaceWidth, surfaceHeight);
}

// Note: This will run on the emulator thread, we don't want any Qt calls happening there.
void ScreenWidgetGL::resizeSurface(u32 width, u32 height) {
	if (previousWidth != width || previousHeight != height) {
		if (glContext) {
			glContext->ResizeSurface(width, height);
		}
	}
}

GL::Context* ScreenWidgetGL::getGLContext() { return glContext.get(); }
#else
ScreenWidgetGL::ScreenWidgetGL(API api, ResizeCallback resizeCallback, QWidget* parent) : ScreenWidget(api, resizeCallback, parent) {
	Helpers::panic("OpenGL renderer not supported. Make sure you've compiled with OpenGL support and that you're on a compatible platform");
}

GL::Context* ScreenWidgetGL::getGLContext() { return nullptr; }
bool ScreenWidgetGL::createContext() { return false; }
void ScreenWidgetGL::resizeDisplay() {}
void ScreenWidgetGL::resizeSurface(u32 width, u32 height) {}
#endif
