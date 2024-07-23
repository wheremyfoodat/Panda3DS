#pragma once
#include <QWidget>
#include <functional>
#include <memory>

#include "gl/context.h"
#include "window_info.h"

// OpenGL widget for drawing the 3DS screen
class ScreenWidget : public QWidget {
	Q_OBJECT

  public:
	using ResizeCallback = std::function<void(u32, u32)>;

	ScreenWidget(ResizeCallback resizeCallback, QWidget* parent = nullptr);
	void resizeEvent(QResizeEvent* event) override;
	// Called by the emulator thread for resizing the actual GL surface, since the emulator thread owns the GL context
	void resizeSurface(u32 width, u32 height);

	GL::Context* getGLContext() { return glContext.get(); }

	// Dimensions of our output surface
	u32 surfaceWidth = 0;
	u32 surfaceHeight = 0;
	WindowInfo windowInfo;

	// Cached "previous" dimensions, used when resizing our window
	u32 previousWidth = 0;
	u32 previousHeight = 0;

  private:
	std::unique_ptr<GL::Context> glContext = nullptr;
	ResizeCallback resizeCallback;

	bool createGLContext();

	qreal devicePixelRatioFromScreen() const;
	int scaledWindowWidth() const;
	int scaledWindowHeight() const;
	std::optional<WindowInfo> getWindowInfo();
};
