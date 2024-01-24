#include "applets/software_keyboard.hpp"

#include <cstring>

#include "kernel/handles.hpp"

using namespace Applets;

void SoftwareKeyboardApplet::reset() {}

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

		default: Helpers::panic("Unimplemented swkbd signal %d\n", parameter.signal);
	}

	return Result::Success;
}

Result::HorizonResult SoftwareKeyboardApplet::start(const MemoryBlock& sharedMem, const std::vector<u8>& parameters, u32 appID) {
	if (parameters.size() < sizeof(SoftwareKeyboardConfig)) {
		Helpers::warn("SoftwareKeyboard::Start: Invalid size for keyboard configuration");
		return Result::Success;
	}

	std::memcpy(&config, &parameters[0], sizeof(config));

	// Temporarily hardcode the pressed button to be the firs tone
	switch (config.numButtonsM1) {
		case SoftwareKeyboardButtonConfig::SingleButton: config.returnCode = SoftwareKeyboardResult::D0Click; break;
		case SoftwareKeyboardButtonConfig::DualButton: config.returnCode = SoftwareKeyboardResult::D1Click0; break;
		case SoftwareKeyboardButtonConfig::TripleButton: config.returnCode = SoftwareKeyboardResult::D2Click0; break;
		case SoftwareKeyboardButtonConfig::NoButton: config.returnCode = SoftwareKeyboardResult::None; break;
	}

	if (config.filterFlags & SoftwareKeyboardFilter::Callback) {
		Helpers::warn("Unimplemented software keyboard profanity callback");
	}

	closeKeyboard(appID);
	return Result::Success;
}

void SoftwareKeyboardApplet::closeKeyboard(u32 appID) {
	Applets::Parameter param = Applets::Parameter{
		.senderID = appID,
		.destID = AppletIDs::Application,
		.signal = static_cast<u32>(APTSignal::WakeupByExit),
		.object = 0,
	};

	// Copy software keyboard configuration into the response parameter
	param.data.resize(sizeof(config));
	std::memcpy(&param.data[0], &config, sizeof(config));

	nextParameter = param;
}