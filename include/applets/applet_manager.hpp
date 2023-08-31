#include "applets/mii_selector.hpp"
#include "applets/software_keyboard.hpp"
#include "helpers.hpp"
#include "memory.hpp"
#include "result/result.hpp"

namespace Applets {
	class AppletManager {
		MiiSelectorApplet miiSelector;
		SoftwareKeyboardApplet swkbd;

	  public:
		AppletManager(Memory& mem);
		void reset();
		AppletBase* getApplet(u32 id);
	};
}  // namespace Applets