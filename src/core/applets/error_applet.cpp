#include "applets/error_applet.hpp"

#include "kernel/handles.hpp"

using namespace Applets;

void ErrorApplet::reset() {}

Result::HorizonResult ErrorApplet::start(const MemoryBlock* sharedMem, const std::vector<u8>& parameters, u32 appID) {
	Applets::Parameter param = Applets::Parameter{
		.senderID = appID,
		.destID = AppletIDs::Application,
		.signal = static_cast<u32>(APTSignal::WakeupByExit),
		.object = 0,
		.data = parameters,  // TODO: Figure out how the data format for this applet
	};

	nextParameter = param;
	return Result::Success;
}

Result::HorizonResult ErrorApplet::receiveParameter(const Applets::Parameter& parameter) {
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