#include "applets/software_keyboard.hpp"

#include "kernel/handles.hpp"

using namespace Applets;

void SoftwareKeyboardApplet::reset() {}
Result::HorizonResult SoftwareKeyboardApplet::start() { return Result::Success; }

Result::HorizonResult SoftwareKeyboardApplet::receiveParameter(const Applets::Parameter& parameter) {
	Helpers::warn("Software keyboard: Unimplemented ReceiveParameter");

	Applets::Parameter param = Applets::Parameter{
		.senderID = parameter.destID,
		.destID = AppletIDs::Application,
		.signal = static_cast<u32>(APTSignal::Response),
		.object = KernelHandles::APTCaptureSharedMemHandle,
		.data = {},
	};

	nextParameter = param;
	return Result::Success;
}