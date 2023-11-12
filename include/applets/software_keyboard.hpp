#include "applets/applet.hpp"

namespace Applets {
	class SoftwareKeyboardApplet final : public AppletBase {
	  public:
		virtual const char* name() override { return "Software Keyboard"; }
		virtual Result::HorizonResult start() override;
		virtual Result::HorizonResult receiveParameter(const Applets::Parameter& parameter) override;
		virtual void reset() override;

		SoftwareKeyboardApplet(Memory& memory, std::optional<Parameter>& nextParam) : AppletBase(memory, nextParam) {}
	};
}  // namespace Applets