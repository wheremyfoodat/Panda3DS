#include "applets/software_keyboard.hpp"

#include <cstring>
#include <string>

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

	// Get keyboard configuration from the application
	std::memcpy(&config, &parameters[0], sizeof(config));

	const std::u16string text = u"Pand";
	u32 textAddress = sharedMem.addr;
	
	// Copy text to shared memory the app gave us
	for (u32 i = 0; i < text.size(); i++) {
		mem.write16(textAddress, u16(text[i]));
		textAddress += sizeof(u16);
	}
	mem.write16(textAddress, 0); // Write UTF-16 null terminator

	// Temporarily hardcode the pressed button to be the firs tone
	switch (config.numButtonsM1) {
		case SoftwareKeyboardButtonConfig::SingleButton: config.returnCode = SoftwareKeyboardResult::D0Click; break;
		case SoftwareKeyboardButtonConfig::DualButton: config.returnCode = SoftwareKeyboardResult::D1Click1; break;
		case SoftwareKeyboardButtonConfig::TripleButton: config.returnCode = SoftwareKeyboardResult::D2Click2; break;
		case SoftwareKeyboardButtonConfig::NoButton: config.returnCode = SoftwareKeyboardResult::None; break;
		default: Helpers::warn("Software keyboard: Invalid button mode specification"); break;
	}

	config.textOffset = 0;
	config.textLength = static_cast<u16>(text.size());
	static_assert(offsetof(SoftwareKeyboardConfig, textOffset) == 324);
	static_assert(offsetof(SoftwareKeyboardConfig, textLength) == 328);

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