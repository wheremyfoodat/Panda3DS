#include "applets/applet.hpp"

namespace Applets {
	class SoftwareKeyboardApplet final : public AppletBase {
	  public:
		virtual const char* name() override { return "Software Keyboard"; }
		virtual Result::HorizonResult start() override;
		virtual Result::HorizonResult receiveParameter() override;
		virtual void reset() override;

		SoftwareKeyboardApplet(Memory& memory) : AppletBase(memory) {}
	};
}  // namespace Applets