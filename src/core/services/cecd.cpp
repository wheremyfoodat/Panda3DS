#include "services/cecd.hpp"
#include "ipc.hpp"
#include "kernel.hpp"

namespace CECDCommands {
	enum : u32 {
		GetInfoEventHandle = 0x000F0000
	};
}

void CECDService::reset() {
	infoEvent = std::nullopt;
}

void CECDService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case CECDCommands::GetInfoEventHandle: getInfoEventHandle(messagePointer); break;
		default: Helpers::panic("CECD service requested. Command: %08X\n", command);
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