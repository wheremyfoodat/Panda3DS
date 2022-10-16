#include "services/cecd.hpp"

namespace CECDCommands {
	enum : u32 {
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
		default: Helpers::panic("CECD service requested. Command: %08X\n", command);
	}
}