#include "panda_qt/screen/screen_mtl.hpp"

#ifdef PANDA3DS_ENABLE_METAL
ScreenWidgetMTL::ScreenWidgetMTL(API api, ResizeCallback resizeCallback, QWidget* parent) : ScreenWidget(api, resizeCallback, parent) {
	if (!createContext()) {
		Helpers::panic("Failed to create Metal context for display");
	}

	resize(800, 240 * 4);
	show();
}

void ScreenWidgetMTL::resizeDisplay() {
	resizeMetalView();
	resizeCallback(surfaceWidth, surfaceHeight);
}

bool ScreenWidgetMTL::createContext() { return createMetalContext(); }
void* ScreenWidgetMTL::getMTKLayer() { return mtkLayer; }

#else
ScreenWidgetMTL::ScreenWidgetMTL(API api, ResizeCallback resizeCallback, QWidget* parent) : ScreenWidget(api, resizeCallback, parent) {
	Helpers::panic("Metal renderer not supported. Make sure you've compiled with Metal support and that you're on a compatible platform");
}

ScreenWidgetMTL::~ScreenWidgetMTL() {}
bool ScreenWidgetMTL::createContext() { return false; }
bool ScreenWidgetMTL::createMetalContext() { return false; }
void* ScreenWidgetMTL::getMTKLayer() { return nullptr; }

void ScreenWidgetMTL::resizeDisplay() {}
void ScreenWidgetMTL::resizeMetalView() {}
#endif