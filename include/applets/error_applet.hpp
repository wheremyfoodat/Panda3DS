#include <string>

#include "applets/applet.hpp"

namespace Applets {
	class ErrorApplet final : public AppletBase {
	  public:
		virtual const char* name() override { return "Error/EULA Agreement"; }
		virtual Result::HorizonResult start(const MemoryBlock* sharedMem, const std::vector<u8>& parameters, u32 appID) override;
		virtual Result::HorizonResult receiveParameter(const Applets::Parameter& parameter) override;
		virtual void reset() override;

		ErrorApplet(Memory& memory, std::optional<Parameter>& nextParam) : AppletBase(memory, nextParam) {}
	};
}  // namespace Applets