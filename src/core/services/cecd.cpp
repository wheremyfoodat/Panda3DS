#include "services/cecd.hpp"

#include "ipc.hpp"
#include "kernel.hpp"

namespace CECDCommands {
	enum : u32 {
		Stop = 0x000C0040,
		GetInfoEventHandle = 0x000F0000,
		GetChangeStateEventHandle = 0x00100000,
		OpenAndRead = 0x00120104,
	};
}

void CECDService::reset() {
	changeStateEvent = std::nullopt;
	infoEvent = std::nullopt;
}

void CECDService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);

	switch (command) {
		case CECDCommands::GetInfoEventHandle: getInfoEventHandle(messagePointer); break;
		case CECDCommands::GetChangeStateEventHandle: getChangeStateEventHandle(messagePointer); break;
		case CECDCommands::OpenAndRead: openAndRead(messagePointer); break;
		case CECDCommands::Stop: stop(messagePointer); break;

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
	mem.write32(messagePointer + 12, *infoEvent);
}

void CECDService::getChangeStateEventHandle(u32 messagePointer) {
	log("CECD::GetChangeStateEventHandle (stubbed)\n");

	if (!changeStateEvent.has_value()) {
		changeStateEvent = kernel.makeEvent(ResetType::OneShot);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x10, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	// TODO: Translation descriptor here?
	mem.write32(messagePointer + 12, *changeStateEvent);
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
	mem.write32(messagePointer + 8, 0);  // Bytes read
}

void CECDService::stop(u32 messagePointer) {
	log("CECD::Stop (stubbed)\n");

	if (changeStateEvent.has_value()) {
		kernel.signalEvent(*changeStateEvent);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x0C, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}
