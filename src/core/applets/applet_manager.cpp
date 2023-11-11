#include "applets/applet_manager.hpp"

#include "services/apt.hpp"

using namespace Applets;

AppletManager::AppletManager(Memory& mem) : miiSelector(mem, nextParameter), swkbd(mem, nextParameter) {}

void AppletManager::reset() {
	miiSelector.reset();
	swkbd.reset();
}

AppletBase* AppletManager::getApplet(u32 id) {
	switch (id) {
		case AppletIDs::MiiSelector:
		case AppletIDs::MiiSelector2: return &miiSelector;

		case AppletIDs::SoftwareKeyboard:
		case AppletIDs::SoftwareKeyboard2: return &swkbd;

		default: return nullptr;
	}
}

Applets::Parameter AppletManager::glanceParameter() {
	if (nextParameter) {
		// Copy parameter
		Applets::Parameter param = nextParameter.value();
		// APT module clears next parameter even for GlanceParameter for these 2 signals
		if (param.signal == APTSignal::DspWakeup || param.signal == APTSignal::DspSleep) {
			nextParameter = std::nullopt;
		}

		return param;
	}

	// Default return value. This is legacy code from before applets were implemented. TODO: Update it
	else {
		return Applets::Parameter{.senderID = 0, .destID = Applets::AppletIDs::Application, .signal = APTSignal::Wakeup, .data = {}};
	}
}

Applets::Parameter AppletManager::receiveParameter() {
	Applets::Parameter param = glanceParameter();
	// ReceiveParameter always clears nextParameter whereas glanceParameter does not
	nextParameter = std::nullopt;

	return param;
}