#include "services/mic.hpp"

namespace MICCommands {
	enum : u32 {
	};
}

namespace Result {
	enum : u32 {
		Success = 0,
	};
}

void MICService::reset() {}

void MICService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		default: Helpers::panic("MIC service requested. Command: %08X\n", command);
	}
}