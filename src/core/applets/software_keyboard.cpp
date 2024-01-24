#include "applets/software_keyboard.hpp"

#include "kernel/handles.hpp"

using namespace Applets;

void SoftwareKeyboardApplet::reset() {}
Result::HorizonResult SoftwareKeyboardApplet::start(const MemoryBlock& sharedMem, const std::vector<u8>& parameters) { return Result::Success; }

Result::HorizonResult SoftwareKeyboardApplet::receiveParameter(const Applets::Parameter& parameter) {
	Helpers::warn("Software keyboard: Unimplemented ReceiveParameter");

	switch (parameter.signal) {
		// Signal == request -> Applet is asking swkbd for a shared memory handle for backing up the framebuffer before opening the applet
		case u32(APTSignal::Request): {
			Applets::Parameter param = Applets::Parameter{
				.senderID = parameter.destID,
				.destID = AppletIDs::Application,
				.signal = static_cast<u32>(APTSignal::Response),
				.object = KernelHandles::APTCaptureSharedMemHandle,
				.data = {},
			};

			nextParameter = param;
			break;
		}

		default: Helpers::panic("SWKBD SIGNAL %d\n", parameter.signal);
	}

	return Result::Success;
}