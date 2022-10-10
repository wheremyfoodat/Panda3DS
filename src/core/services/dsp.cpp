#include "services/dsp.hpp"

namespace DSPCommands {
	enum : u32 {
	};
}

namespace Result {
	enum : u32 {
		Success = 0,
	};
}

void DSPService::reset() {}

void DSPService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		default: Helpers::panic("LCD service requested. Command: %08X\n", command);
	}
}