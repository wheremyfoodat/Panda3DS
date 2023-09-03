#include "applets/applet.hpp"

namespace Applets {
	class MiiSelectorApplet final : public AppletBase {
	  public:
		virtual const char* name() override { return "Mii Selector"; }
		virtual Result::HorizonResult start() override;
		virtual Result::HorizonResult receiveParameter() override;
		virtual void reset() override;

		MiiSelectorApplet(Memory& memory) : AppletBase(memory) {}
	};
}  // namespace Applets