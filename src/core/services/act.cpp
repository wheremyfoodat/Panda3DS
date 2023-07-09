#include "services/act.hpp"
#include "ipc.hpp"

namespace ACTCommands {
	enum : u32 {
		Initialize = 0x00010084,
		GenerateUUID = 0x000D0040
	};
}

void ACTService::reset() {}

void ACTService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case ACTCommands::GenerateUUID: generateUUID(messagePointer); break;
		case ACTCommands::Initialize: initialize(messagePointer); break;
		default: Helpers::panic("ACT service requested. Command: %08X\n", command);
	}
}

void ACTService::initialize(u32 messagePointer) {
	log("ACT::Initialize");

	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void ACTService::generateUUID(u32 messagePointer) {
	log("ACT::GenerateUUID (stubbed)\n");
	
	// TODO: The header is probably wrong
	mem.write32(messagePointer, IPC::responseHeader(0xD, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}