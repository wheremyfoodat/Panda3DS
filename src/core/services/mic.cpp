#include "services/mic.hpp"

namespace MICCommands {
	enum : u32 {
		MapSharedMem = 0x00010042
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
		case MICCommands::MapSharedMem: mapSharedMem(messagePointer); break;
		default: Helpers::panic("MIC service requested. Command: %08X\n", command);
	}
}

void MICService::mapSharedMem(u32 messagePointer) {
	u32 size = mem.read32(messagePointer + 4);
	u32 handle = mem.read32(messagePointer + 12);

	log("MIC::MapSharedMem (size = %08X, handle = %X) (stubbed)\n", size, handle);
	mem.write32(messagePointer + 4, Result::Success);
}