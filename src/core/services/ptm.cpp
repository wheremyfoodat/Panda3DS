#include "services/ptm.hpp"

namespace PTMCommands {
	enum : u32 {
	};
}

namespace Result {
	enum : u32 {
		Success = 0,
	};
}

void PTMService::reset() {}

void PTMService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		default: Helpers::panic("PTM service requested. Command: %08X\n", command);
	}
}