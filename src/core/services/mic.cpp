#include "services/mic.hpp"

namespace MICCommands {
	enum : u32 {
		MapSharedMem = 0x00010042,
		SetGain = 0x00080040,
		GetGain = 0x00090000
	};
}

namespace Result {
	enum : u32 {
		Success = 0,
	};
}

void MICService::reset() {
	gain = 0;
}

void MICService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case MICCommands::GetGain: getGain(messagePointer); break;
		case MICCommands::MapSharedMem: mapSharedMem(messagePointer); break;
		case MICCommands::SetGain: setGain(messagePointer); break;
		default: Helpers::panic("MIC service requested. Command: %08X\n", command);
	}
}

void MICService::mapSharedMem(u32 messagePointer) {
	u32 size = mem.read32(messagePointer + 4);
	u32 handle = mem.read32(messagePointer + 12);

	log("MIC::MapSharedMem (size = %08X, handle = %X) (stubbed)\n", size, handle);
	mem.write32(messagePointer + 4, Result::Success);
}

void MICService::getGain(u32 messagePointer) {
	log("MIC::GetGain\n");
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, gain);
}

void MICService::setGain(u32 messagePointer) {
	gain = mem.read8(messagePointer + 4);
	log("MIC::SetGain (value = %d)\n", gain);

	mem.write32(messagePointer + 4, Result::Success);
}