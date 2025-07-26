#pragma once
#include "panda_qt/screen/screen.hpp"

class ScreenWidgetMTL : public ScreenWidget {
	void* mtkLayer = nullptr;

	// Objective-C++ functions for handling the Metal context
	bool createMetalContext();
	void resizeMetalView();

  public:
	ScreenWidgetMTL(API api, ResizeCallback resizeCallback, QWidget* parent = nullptr);

	virtual void* getMTKLayer() override;
	virtual bool createContext() override;
	virtual void resizeDisplay() override;
};