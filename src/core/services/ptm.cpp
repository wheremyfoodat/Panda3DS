#include "services/ptm.hpp"
#include "ipc.hpp"

namespace PTMCommands {
	enum : u32 {
		GetStepHistory = 0x000B00C2, 
		GetTotalStepCount = 0x000C0000,
		ConfigureNew3DSCPU = 0x08180040
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
		case PTMCommands::ConfigureNew3DSCPU: configureNew3DSCPU(messagePointer); break;
		case PTMCommands::GetStepHistory: getStepHistory(messagePointer); break;
		case PTMCommands::GetTotalStepCount: getTotalStepCount(messagePointer); break;
		default: Helpers::panic("PTM service requested. Command: %08X\n", command);
	}
}

void PTMService::getStepHistory(u32 messagePointer) {
	log("PTM::GetStepHistory [stubbed]\n");
	mem.write32(messagePointer, IPC::responseHeader(0xB, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
}

void PTMService::getTotalStepCount(u32 messagePointer) {
	log("PTM::GetTotalStepCount\n");
	mem.write32(messagePointer, IPC::responseHeader(0xC, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 3); // We walk a lot
}

void PTMService::configureNew3DSCPU(u32 messagePointer) {
	log("PTM::ConfigureNew3DSCPU [stubbed]\n");
	mem.write32(messagePointer, IPC::responseHeader(0x818, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}