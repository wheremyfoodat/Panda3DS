#pragma once
#include <optional>

#include "applets/mii_selector.hpp"
#include "applets/software_keyboard.hpp"
#include "helpers.hpp"
#include "memory.hpp"
#include "result/result.hpp"

namespace Applets {
	class AppletManager {
		MiiSelectorApplet miiSelector;
		SoftwareKeyboardApplet swkbd;
		std::optional<Applets::Parameter> nextParameter = std::nullopt;

	  public:
		AppletManager(Memory& mem);
		void reset();
		AppletBase* getApplet(u32 id);

		Applets::Parameter glanceParameter();
		Applets::Parameter receiveParameter();
	};
}  // namespace Applets