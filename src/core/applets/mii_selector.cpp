#include "applets/mii_selector.hpp"

using namespace Applets;

void MiiSelectorApplet::reset() {}
Result::HorizonResult MiiSelectorApplet::start() { return Result::Success; }

Result::HorizonResult MiiSelectorApplet::receiveParameter() {
	Helpers::warn("Mii Selector: Unimplemented ReceiveParameter");
	return Result::Success;
}