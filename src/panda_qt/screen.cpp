#include "opengl.hpp"
// opengl.hpp must be included at the very top. This comment exists to make clang-format not reorder it :p
#include <QGuiApplication>
#include <QScreen>
#include <QWindow>
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

#if !defined(_WIN32) && !defined(APPLE)
#include <qpa/qplatformnativeinterface.h>
#endif

#include "panda_qt/screen.hpp"

// OpenGL screen widget, based on https://github.com/stenzek/duckstation/blob/master/src/duckstation-qt/displaywidget.cpp
// and https://github.com/melonDS-emu/melonDS/blob/master/src/frontend/qt_sdl/main.cpp

#ifdef PANDA3DS_ENABLE_OPENGL
ScreenWidget::ScreenWidget(ResizeCallback resizeCallback, QWidget* parent) : QWidget(parent), resizeCallback(resizeCallback) {
	// Create a native window for use with our graphics API of choice
	resize(800, 240 * 4);
	
	setAutoFillBackground(false);
	setAttribute(Qt::WA_NativeWindow, true);
	setAttribute(Qt::WA_NoSystemBackground, true);
	setAttribute(Qt::WA_PaintOnScreen, true);
	setAttribute(Qt::WA_KeyCompression, false);
	setFocusPolicy(Qt::StrongFocus);
	setMouseTracking(true);

	if (!createGLContext()) {
		Helpers::panic("Failed to create GL context for display");
	}
}

void ScreenWidget::resizeEvent(QResizeEvent* event) {
	previousWidth = surfaceWidth;
	previousHeight = surfaceHeight;
	QWidget::resizeEvent(event);

	// Update surfaceWidth/surfaceHeight following the resize
	std::optional<WindowInfo> windowInfo = getWindowInfo();
	if (windowInfo) {
		this->windowInfo = *windowInfo;
	}

	// This will call take care of calling resizeSurface from the emulator thread
	resizeCallback(surfaceWidth, surfaceHeight);
}

// Note: This will run on the emulator thread, we don't want any Qt calls happening there.
void ScreenWidget::resizeSurface(u32 width, u32 height) {
	if (previousWidth != width || previousHeight != height) {
		if (glContext) {
			glContext->ResizeSurface(width, height);
		}
	}
}

bool ScreenWidget::createGLContext() {
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

qreal ScreenWidget::devicePixelRatioFromScreen() const {
	const QScreen* screenForRatio = window()->windowHandle()->screen();
	if (!screenForRatio) {
		screenForRatio = QGuiApplication::primaryScreen();
	}

	return screenForRatio ? screenForRatio->devicePixelRatio() : static_cast<qreal>(1);
}

int ScreenWidget::scaledWindowWidth() const {
	return std::max(static_cast<int>(std::ceil(static_cast<qreal>(width()) * devicePixelRatioFromScreen())), 1);
}

int ScreenWidget::scaledWindowHeight() const {
	return std::max(static_cast<int>(std::ceil(static_cast<qreal>(height()) * devicePixelRatioFromScreen())), 1);
}

std::optional<WindowInfo> ScreenWidget::getWindowInfo() {
	WindowInfo wi;

// Windows and Apple are easy here since there's no display connection.
#if defined(_WIN32)
	wi.type = WindowInfo::Type::Win32;
	wi.window_handle = reinterpret_cast<void*>(winId());
#elif defined(__APPLE__)
	wi.type = WindowInfo::Type::MacOS;
	wi.window_handle = reinterpret_cast<void*>(winId());
#else
	QPlatformNativeInterface* pni = QGuiApplication::platformNativeInterface();
	const QString platform_name = QGuiApplication::platformName();
	if (platform_name == QStringLiteral("xcb")) {
		wi.type = WindowInfo::Type::X11;
		wi.display_connection = pni->nativeResourceForWindow("display", windowHandle());
		wi.window_handle = reinterpret_cast<void*>(winId());
	} else if (platform_name == QStringLiteral("wayland")) {
		wi.type = WindowInfo::Type::Wayland;
		QWindow* handle = windowHandle();
		if (handle == nullptr) {
			return std::nullopt;
		}

		wi.display_connection = pni->nativeResourceForWindow("display", handle);
		wi.window_handle = pni->nativeResourceForWindow("surface", handle);
	} else {
		qCritical() << "Unknown PNI platform " << platform_name;
		return std::nullopt;
	}
#endif

	wi.surface_width = static_cast<u32>(scaledWindowWidth());
	wi.surface_height = static_cast<u32>(scaledWindowHeight());
	wi.surface_scale = static_cast<float>(devicePixelRatioFromScreen());

	surfaceWidth = wi.surface_width;
	surfaceHeight = wi.surface_height;

	return wi;
}
#endif
