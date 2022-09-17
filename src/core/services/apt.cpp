#include "services/apt.hpp"

namespace APTCommands {
	enum : u32 {
		GetLockHandle = 0x00010040
	};
}

namespace Result {
	enum : u32 {
		Success = 0,
		Failure = 0xFFFFFFFF
	};
}

void APTService::reset() {}

void APTService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case APTCommands::GetLockHandle: getLockHandle(messagePointer); break;
		default: Helpers::panic("APT service requested. Command: %08X\n", command);
	}
}

void APTService::getLockHandle(u32 messagePointer) {
	printf("APT: getLockHandle (Failure)\n");
	mem.write32(messagePointer + 4, Result::Failure); // Result code
	mem.write32(messagePointer + 16, 0); // Translation descriptor
}