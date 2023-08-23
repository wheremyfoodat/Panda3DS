#include "applets/software_keyboard.hpp"

using namespace Applets;

void SoftwareKeyboardApplet::reset() {}
Result::HorizonResult SoftwareKeyboardApplet::start() { return Result::Success; }

Result::HorizonResult SoftwareKeyboardApplet::receiveParameter() {
	Helpers::warn("Software keyboard: Unimplemented ReceiveParameter");
	return Result::Success;
}