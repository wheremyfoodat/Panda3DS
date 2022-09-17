#include "services/hid.hpp"

namespace HIDCommands {
	enum : u32 {
		GetIPCHandles = 0x000A0000
	};
}

namespace Result {
	enum : u32 {
		Success = 0,
		Failure = 0xFFFFFFFF
	};
}

void HIDService::reset() {}

void HIDService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case HIDCommands::GetIPCHandles: getIPCHandles(messagePointer); break;
		default: Helpers::panic("HID service requested. Command: %08X\n", command);
	}
}

void HIDService::getIPCHandles(u32 messagePointer) {
	printf("HID: getIPCHandles (Failure)\n");
	mem.write32(messagePointer + 4, Result::Failure); // Result code
	mem.write32(messagePointer + 8, 0x14000000); // Translation descriptor
}