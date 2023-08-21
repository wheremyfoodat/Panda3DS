#include "services/cecd.hpp"

#include "ipc.hpp"
#include "kernel.hpp"

namespace CECDCommands {
	enum : u32 {
		GetInfoEventHandle = 0x000F0000,
		OpenAndRead = 0x00120104,
	};
}

void CECDService::reset() { infoEvent = std::nullopt; }

void CECDService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case CECDCommands::GetInfoEventHandle: getInfoEventHandle(messagePointer); break;
		case CECDCommands::OpenAndRead: openAndRead(messagePointer); break;
		default:
			Helpers::panicDev("CECD service requested. Command: %08X\n", command);
			mem.write32(messagePointer + 4, Result::Success);
			break;
	}
}

void CECDService::getInfoEventHandle(u32 messagePointer) {
	log("CECD::GetInfoEventHandle (stubbed)\n");

	if (!infoEvent.has_value()) {
		infoEvent = kernel.makeEvent(ResetType::OneShot);
	}

	mem.write32(messagePointer, IPC::responseHeader(0xF, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	// TODO: Translation descriptor here?
	mem.write32(messagePointer + 12, infoEvent.value());
}

void CECDService::openAndRead(u32 messagePointer) {
	const u32 bufferSize = mem.read32(messagePointer + 4);
	const u32 programID = mem.read32(messagePointer + 8);
	const u32 pathType = mem.read32(messagePointer + 12);
	const u32 bufferAddress = mem.read32(messagePointer + 32);
	log("CECD::OpenAndRead (size = %08X, address = %08X, path type = %d)\n", bufferSize, bufferAddress, pathType);

	// TODO: We should implement this properly the time comes
	mem.write32(messagePointer, IPC::responseHeader(0x12, 2, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0); // Bytes read
}