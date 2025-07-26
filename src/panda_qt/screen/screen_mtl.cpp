#include "panda_qt/screen/screen_mtl.hpp"

ScreenWidgetMTL::ScreenWidgetMTL(API api, ResizeCallback resizeCallback, QWidget* parent) : ScreenWidget(api, resizeCallback, parent) {
	if (!createContext()) {
		Helpers::panic("Failed to create Metal context for display");
	}

	resize(800, 240 * 4);
	show();
}

void ScreenWidgetMTL::resizeDisplay() { resizeMetalView(); }
bool ScreenWidgetMTL::createContext() { return createMetalContext(); }
void* ScreenWidgetMTL::getMTKLayer() { return mtkLayer; }