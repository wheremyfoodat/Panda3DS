#include "applets/software_keyboard.hpp"

using namespace Applets;

void SoftwareKeyboardApplet::reset() {}
Result::HorizonResult SoftwareKeyboardApplet::start() { return Result::Success; }

Result::HorizonResult SoftwareKeyboardApplet::receiveParameter() {
	Helpers::panic("Software keyboard received parameter");
	return Result::Success;
}