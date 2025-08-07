#pragma once
#include <memory>

#include "gl/context.h"
#include "panda_qt/screen/screen.hpp"

class ScreenWidgetGL : public ScreenWidget {
	std::unique_ptr<GL::Context> glContext = nullptr;

  public:
	ScreenWidgetGL(API api, ResizeCallback resizeCallback, QWidget* parent = nullptr);

	virtual GL::Context* getGLContext() override;
	virtual bool createContext() override;

	virtual void resizeDisplay() override;
	virtual void resizeSurface(u32 width, u32 height) override;
};