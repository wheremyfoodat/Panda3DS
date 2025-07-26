#pragma once
#include <QWidget>
#include <functional>

#include "gl/context.h"
#include "screen_layout.hpp"
#include "window_info.h"

// Abstract screen widget for drawing the 3DS screen. We've got a child class for each graphics API (ScreenWidgetGL, ScreenWidgetMTL, ...)
class ScreenWidget : public QWidget {
	Q_OBJECT

  public:
	using ResizeCallback = std::function<void(u32, u32)>;

	enum class API { OpenGL, Metal, Vulkan };

	ScreenWidget(API api, ResizeCallback resizeCallback, QWidget* parent = nullptr);
	virtual ~ScreenWidget() {}

	void resizeEvent(QResizeEvent* event) override;

	virtual GL::Context* getGLContext() { return nullptr; }
	virtual void* getMTKLayer() { return nullptr; }

	// Dimensions of our output surface
	u32 surfaceWidth = 0;
	u32 surfaceHeight = 0;
	WindowInfo windowInfo;

	// Cached "previous" dimensions, used when resizing our window
	u32 previousWidth = 0;
	u32 previousHeight = 0;

	API api = API::OpenGL;

	// Coordinates (x/y/width/height) for the two screens in window space, used for properly handling touchscreen
	ScreenLayout::WindowCoordinates screenCoordinates;
	// Screen layouts and sizes
	ScreenLayout::Layout screenLayout = ScreenLayout::Layout::Default;
	float topScreenSize = 0.5f;

	void reloadScreenLayout(ScreenLayout::Layout newLayout, float newTopScreenSize);

	// Creates a screen widget depending on the graphics API we're using
	static ScreenWidget* getWidget(API api, ResizeCallback resizeCallback, QWidget* parent = nullptr);

	// Called by the emulator thread on OpenGL for resizing the actual GL surface, since the emulator thread owns the GL context
	virtual void resizeSurface(u32 width, u32 height) {};

  protected:
	ResizeCallback resizeCallback;

	virtual bool createContext() = 0;
	virtual void resizeDisplay() = 0;
	std::optional<WindowInfo> getWindowInfo();

  private:
	qreal devicePixelRatioFromScreen() const;
	int scaledWindowWidth() const;
	int scaledWindowHeight() const;

	void reloadScreenCoordinates();
};
