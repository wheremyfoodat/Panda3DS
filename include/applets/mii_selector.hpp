#include "applets/applet.hpp"

class MiiSelectorApplet final : public AppletBase {
	virtual Result::HorizonResult start() override;
	virtual void reset() override;

	MiiSelectorApplet(Memory& memory) : AppletBase(memory) {}
};