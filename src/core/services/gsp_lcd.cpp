#include "services/gsp_lcd.hpp"
#include "ipc.hpp"

namespace LCDCommands {
	enum : u32 {
		SetLedForceOff = 0x00130040,
	};
}

void LCDService::reset() {}

void LCDService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case LCDCommands::SetLedForceOff: setLedForceOff(messagePointer); break;

		default: Helpers::panic("LCD service requested. Command: %08X\n", command);
	}
}

void LCDService::setLedForceOff(u32 messagePointer) {
	const u8 state = mem.read8(messagePointer + 4);
	log("LCD::SetLedForceOff (state = %X)\n", state);

	mem.write32(messagePointer, IPC::responseHeader(0x13, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}