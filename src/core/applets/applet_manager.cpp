#include "applets/applet_manager.hpp"
using namespace Applets;

AppletManager::AppletManager(Memory& mem) : miiSelector(mem), swkbd(mem) {}

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