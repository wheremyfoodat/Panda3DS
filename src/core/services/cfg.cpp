#include "services/cfg.hpp"

namespace CFGCommands {
	enum : u32 {
	};
}

namespace Result {
	enum : u32 {
		Success = 0,
	};
}

void CFGService::reset() {}

void CFGService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		default: Helpers::panic("CFG service requested. Command: %08X\n", command);
	}
}