#include "services/ir_user.hpp"

#include "ipc.hpp"
#include "kernel.hpp"

namespace IRUserCommands {
	enum : u32 {
		FinalizeIrnop = 0x00020000,
		RequireConnection = 0x00060040,
		Disconnect = 0x00090000,
		GetConnectionStatusEvent = 0x000C0000,
		InitializeIrnopShared = 0x00180182
	};
}

void IRUserService::reset() { connectionStatusEvent = std::nullopt; }

void IRUserService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case IRUserCommands::Disconnect: disconnect(messagePointer); break;
		case IRUserCommands::FinalizeIrnop: finalizeIrnop(messagePointer); break;
		case IRUserCommands::GetConnectionStatusEvent: getConnectionStatusEvent(messagePointer); break;
		case IRUserCommands::InitializeIrnopShared: initializeIrnopShared(messagePointer); break;
		case IRUserCommands::RequireConnection: requireConnection(messagePointer); break;
		default: Helpers::panic("ir:USER service requested. Command: %08X\n", command);
	}
}

void IRUserService::initializeIrnopShared(u32 messagePointer) {
	const u32 sharedMemSize = mem.read32(messagePointer + 4);
	const u32 receiveBufferSize = mem.read32(messagePointer + 8);
	const u32 receiveBufferPackageCount = mem.read32(messagePointer + 12);
	const u32 sendBufferSize = mem.read32(messagePointer + 16);
	const u32 sendBufferPackageCount = mem.read32(messagePointer + 20);
	const u32 bitrate = mem.read32(messagePointer + 24);
	const u32 descriptor = mem.read32(messagePointer + 28);
	const u32 sharedMemHandle = mem.read32(messagePointer + 32);

	log("IR:USER: InitializeIrnopShared (shared mem size = %08X, sharedMemHandle = %X) (stubbed)\n", sharedMemSize, sharedMemHandle);
	Helpers::warn("Game is initializing IR:USER. If it explodes, this is probably why");

	mem.write32(messagePointer, IPC::responseHeader(0x18, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void IRUserService::finalizeIrnop(u32 messagePointer) {
	log("IR:USER: FinalizeIrnop\n");

	// This should disconnect any connected device de-initialize the shared memory
	mem.write32(messagePointer, IPC::responseHeader(0x2, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void IRUserService::getConnectionStatusEvent(u32 messagePointer) {
	log("IR:USER: GetConnectionStatusEvent\n");

	if (!connectionStatusEvent.has_value()) {
		connectionStatusEvent = kernel.makeEvent(ResetType::OneShot);
	}

	mem.write32(messagePointer, IPC::responseHeader(0xC, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	// TOOD: Descriptor here
	mem.write32(messagePointer + 12, connectionStatusEvent.value());
}

void IRUserService::requireConnection(u32 messagePointer) {
	const u8 deviceID = mem.read8(messagePointer + 4);
	log("IR:USER: RequireConnection (device: %d)\n", deviceID);

	mem.write32(messagePointer, IPC::responseHeader(0x6, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void IRUserService::disconnect(u32 messagePointer) {
	log("IR:USER: Disconnect\n");
	mem.write32(messagePointer, IPC::responseHeader(0x9, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}