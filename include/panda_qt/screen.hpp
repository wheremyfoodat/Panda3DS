#pragma once
#include <QWidget>
#include <memory>

#include "gl/context.h"
#include "window_info.h"

// OpenGL widget for drawing the 3DS screen
class ScreenWidget : public QWidget {
	Q_OBJECT

  public:
	ScreenWidget(QWidget* parent = nullptr);
	GL::Context* getGLContext() { return glContext.get(); }

	// Dimensions of our output surface
	u32 surfaceWidth = 0;
	u32 surfaceHeight = 0;

  private:
	std::unique_ptr<GL::Context> glContext = nullptr;
	bool createGLContext();

	qreal devicePixelRatioFromScreen() const;
	int scaledWindowWidth() const;
	int scaledWindowHeight() const;
	std::optional<WindowInfo> getWindowInfo();
};
