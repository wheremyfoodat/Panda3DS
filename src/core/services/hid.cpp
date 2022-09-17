#include "services/hid.hpp"

namespace HIDCommands {
	enum : u32 {
		
	};
}

namespace Result {
	enum : u32 {
		Success = 0,
	};
}

void HIDService::reset() {}

void HIDService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		default: Helpers::panic("HID service requested. Command: %08X\n", command);
	}
}