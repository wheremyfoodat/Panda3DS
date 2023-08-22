#include "applets/mii_selector.hpp"

using namespace Applets;

void MiiSelectorApplet::reset() {}
Result::HorizonResult MiiSelectorApplet::start() { return Result::Success; }

Result::HorizonResult MiiSelectorApplet::receiveParameter() {
	Helpers::panic("Mii Selector received parameter");
	return Result::Success;
}