#include "services/gsp_lcd.hpp"
#include "ipc.hpp"

namespace LCDCommands {
	enum : u32 {
	};
}

void LCDService::reset() {}

void LCDService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	Helpers::panic("LCD service requested. Command: %08X\n", command);
}
