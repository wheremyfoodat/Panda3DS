#include "services/cecd.hpp"

namespace CECDCommands {
	enum : u32 {
		GetEventHandle = 0x000F0000
	};
}

namespace Result {
	enum : u32 {
		Success = 0,
	};
}

void CECDService::reset() {}

void CECDService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case CECDCommands::GetEventHandle: getEventHandle(messagePointer); break;
		default: Helpers::panic("CECD service requested. Command: %08X\n", command);
	}
}

void CECDService::getEventHandle(u32 messagePointer) {
	log("CECD::GetEventHandle (stubbed)\n");
	Helpers::panic("TODO: Actually implement CECD::GetEventHandle");

	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 12, 0x66666666);
}