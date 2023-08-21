#include "services/act.hpp"
#include "ipc.hpp"

namespace ACTCommands {
	enum : u32 {
		Initialize = 0x00010084,
		GetAccountDataBlock = 0x000600C2,
		GenerateUUID = 0x000D0040,
	};
}

void ACTService::reset() {}

void ACTService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case ACTCommands::GenerateUUID: generateUUID(messagePointer); break;
		case ACTCommands::GetAccountDataBlock: getAccountDataBlock(messagePointer); break;
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

void ACTService::getAccountDataBlock(u32 messagePointer) {
	log("ACT::GetAccountDataBlock (stubbed)\n");

	const u32 size = mem.read32(messagePointer + 8);
	const u32 blkID = mem.read32(messagePointer + 12);
	const u32 outputPointer = mem.read32(messagePointer + 20);

	// TODO: This header is probably also wrong
	// Also we need to populate the data block here. Half of it is undocumented though >_<
	mem.write32(messagePointer, IPC::responseHeader(0x6, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}