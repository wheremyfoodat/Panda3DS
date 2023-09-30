#pragma once
#include <QWidget>
#include <memory>

#include "gl/context.h"
#include "window_info.h"

// OpenGL widget for drawing the 3DS screen
class ScreenWidget : public QWidget {
	Q_OBJECT

  public:
	ScreenWidget(QWidget* parent = nullptr) : QWidget(parent) {
		// Create a native window for use with our graphics API of choice
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

  private:
	std::unique_ptr<GL::Context> glContext = nullptr;
	bool createGLContext();

	qreal devicePixelRatioFromScreen() const;
	int scaledWindowWidth() const;
	int scaledWindowHeight() const;
	std::optional<WindowInfo> getWindowInfo();
};
