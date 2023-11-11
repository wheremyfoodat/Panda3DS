#include "applets/software_keyboard.hpp"

using namespace Applets;

void SoftwareKeyboardApplet::reset() {}
Result::HorizonResult SoftwareKeyboardApplet::start() { return Result::Success; }

Result::HorizonResult SoftwareKeyboardApplet::receiveParameter() {
	Helpers::warn("Software keyboard: Unimplemented ReceiveParameter");

	Applets::Parameter param = Applets::Parameter{
		.senderID = AppletIDs::SoftwareKeyboard,
		.destID = AppletIDs::Application,
		.signal = APTSignal::Response,
		.data = {},
	};

	nextParameter = param;
	return Result::Success;
}