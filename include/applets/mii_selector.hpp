#include "applets/applet.hpp"

namespace Applets {
	class MiiSelectorApplet final : public AppletBase {
	  public:
		virtual Result::HorizonResult start() override;
		virtual void reset() override;

		MiiSelectorApplet(Memory& memory) : AppletBase(memory) {}
	};
}  // namespace Applets