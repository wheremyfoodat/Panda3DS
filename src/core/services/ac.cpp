#include "services/ac.hpp"
#include "ipc.hpp"

namespace ACCommands {
	enum : u32 {
		CreateDefaultConfig = 0x00010000,
		CancelConnectAsync = 0x00070002,
		CloseAsync = 0x00080004,
		GetLastErrorCode = 0x000A0000,
		GetStatus = 0x000C0000,
		GetWifiStatus = 0x000D0000,
		GetConnectingInfraPriority = 0x000F0000,
		GetNZoneBeaconNotFoundEvent = 0x002F0004,
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
		case ACCommands::GetConnectingInfraPriority: getConnectingInfraPriority(messagePointer); break;
		case ACCommands::GetLastErrorCode: getLastErrorCode(messagePointer); break;
		case ACCommands::GetNZoneBeaconNotFoundEvent: getNZoneBeaconNotFoundEvent(messagePointer); break;
		case ACCommands::GetStatus: getStatus(messagePointer); break;
		case ACCommands::GetWifiStatus: getWifiStatus(messagePointer); break;
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

void ACService::getConnectingInfraPriority(u32 messagePointer) {
	log("AC::GetConnectingInfraPriority (stubbed)\n");

	// TODO: Find out what this is
	mem.write32(messagePointer, IPC::responseHeader(0x0F, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);
}

void ACService::getStatus(u32 messagePointer) {
	log("AC::GetStatus (stubbed)\n");

	// TODO: Find out what this is
	mem.write32(messagePointer, IPC::responseHeader(0x0C, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);
}

void ACService::getWifiStatus(u32 messagePointer) {
	log("AC::GetWifiStatus (stubbed)\n");

	enum class WifiStatus : u32 {
		None = 0,
		Slot1 = 1,
		Slot2 = 2,
		Slot3 = 4,
	};

	mem.write32(messagePointer, IPC::responseHeader(0x0D, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, static_cast<u32>(WifiStatus::None));
}

void ACService::isConnected(u32 messagePointer) {
	log("AC::IsConnected\n");
	// This has parameters according to the command word but it's unknown what they are

	mem.write32(messagePointer, IPC::responseHeader(0x3E, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, connected ? 1 : 0);
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

void ACService::getNZoneBeaconNotFoundEvent(u32 messagePointer) {
	const u32 processID = mem.read32(messagePointer + 8);
	const Handle event = mem.read32(messagePointer + 16);
	log("AC::GetNZoneBeaconNotFoundEvent (process ID = %X, event = %X) (stubbed)\n", processID, event);

	mem.write32(messagePointer, IPC::responseHeader(0x2F, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}