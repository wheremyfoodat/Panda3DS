#include "applets/mii_selector.hpp"

#include "kernel/handles.hpp"

using namespace Applets;

void MiiSelectorApplet::reset() {}
Result::HorizonResult MiiSelectorApplet::start(const MemoryBlock& sharedMem, const std::vector<u8>& parameters, u32 appID) { return Result::Success; }

Result::HorizonResult MiiSelectorApplet::receiveParameter(const Applets::Parameter& parameter) {
	Helpers::warn("Mii Selector: Unimplemented ReceiveParameter");

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