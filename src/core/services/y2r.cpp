#include "services/y2r.hpp"

namespace Y2RCommands {
	enum : u32 {
	};
}

namespace Result {
	enum : u32 {
		Success = 0,
	};
}

void Y2RService::reset() {}

void Y2RService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		default: Helpers::panic("Y2R service requested. Command: %08X\n", command);
	}
}