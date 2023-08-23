#include "services/ac.hpp"
#include "ipc.hpp"

namespace ACCommands {
	enum : u32 {
		CreateDefaultConfig = 0x00010000,
		CancelConnectAsync = 0x00070002,
		CloseAsync = 0x00080004,
		GetLastErrorCode = 0x000A0000,
		RegisterDisconnectEvent = 0x00300004,
		IsConnected = 0x003E0042,
		SetClientVersion = 0x00400042,
	};
}

void ACService::reset() {
	connected = false;
	disconnectEvent = std::nullopt;
}

void ACService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case ACCommands::CancelConnectAsync: cancelConnectAsync(messagePointer); break;
		case ACCommands::CloseAsync: closeAsync(messagePointer); break;
		case ACCommands::CreateDefaultConfig: createDefaultConfig(messagePointer); break;
		case ACCommands::GetLastErrorCode: getLastErrorCode(messagePointer); break;
		case ACCommands::IsConnected: isConnected(messagePointer); break;
		case ACCommands::RegisterDisconnectEvent: registerDisconnectEvent(messagePointer); break;
		case ACCommands::SetClientVersion: setClientVersion(messagePointer); break;
		default: Helpers::panic("AC service requested. Command: %08X\n", command);
	}
}

void ACService::cancelConnectAsync(u32 messagePointer) {
	log("AC::CancelCommandAsync (stubbed)\n");

	// TODO: Verify if this response header is correct on hardware
	mem.write32(messagePointer, IPC::responseHeader(0x7, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void ACService::closeAsync(u32 messagePointer) {
	log("AC::CloseAsync (stubbed)\n");
	connected = false;

	if (disconnectEvent.has_value()) {
		Helpers::warn("AC::DisconnectEvent should be signalled but isn't implemented yet");
	}

	// TODO: Verify if this response header is correct on hardware
	mem.write32(messagePointer, IPC::responseHeader(0x8, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void ACService::createDefaultConfig(u32 messagePointer) {
	log("AC::CreateDefaultConfig (stubbed)\n");

	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	// TODO: Verify response buffer on hardware
}

void ACService::getLastErrorCode(u32 messagePointer) {
	log("AC::GetLastErrorCode (stubbed)\n");

	mem.write32(messagePointer, IPC::responseHeader(0x0A, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0); // Hopefully this means no error?
}

void ACService::isConnected(u32 messagePointer) {
	log("AC::IsConnected\n");
	// This has parameters according to the command word but it's unknown what they are

	mem.write32(messagePointer, IPC::responseHeader(0x3E, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, connected ? 1 : 0);  // Hopefully this means no error?
}

void ACService::setClientVersion(u32 messagePointer) {
	u32 version = mem.read32(messagePointer + 4);
	log("AC::SetClientVersion (version = %d)\n", version);

	mem.write32(messagePointer, IPC::responseHeader(0x40, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void ACService::registerDisconnectEvent(u32 messagePointer) {
	log("AC::RegisterDisconnectEvent (stubbed)\n");
	const u32 pidHeader = mem.read32(messagePointer + 4);
	const u32 copyHandleHeader = mem.read32(messagePointer + 12);
	// Event signaled when disconnecting from AC. TODO: Properly implement it.
	const Handle eventHandle = mem.read32(messagePointer + 16);

	disconnectEvent = eventHandle;

	mem.write32(messagePointer, IPC::responseHeader(0x30, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}