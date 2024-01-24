#include "applets/applet.hpp"

namespace Applets {
	class MiiSelectorApplet final : public AppletBase {
	  public:
		virtual const char* name() override { return "Mii Selector"; }
		virtual Result::HorizonResult start(const MemoryBlock& sharedMem, const std::vector<u8>& parameters, u32 appID) override;
		virtual Result::HorizonResult receiveParameter(const Applets::Parameter& parameter) override;
		virtual void reset() override;

		MiiSelectorApplet(Memory& memory, std::optional<Parameter>& nextParam) : AppletBase(memory, nextParam) {}
	};
}  // namespace Applets