#include "services/am.hpp"

#include "ipc.hpp"

namespace AMCommands {
	enum : u32 {
		GetDLCTitleInfo = 0x10050084,
		ListTitleInfo = 0x10070102,
		GetPatchTitleInfo = 0x100D0084,
	};
}

void AMService::reset() {}

void AMService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case AMCommands::GetPatchTitleInfo: getPatchTitleInfo(messagePointer); break;
		case AMCommands::GetDLCTitleInfo: getDLCTitleInfo(messagePointer); break;
		case AMCommands::ListTitleInfo: listTitleInfo(messagePointer); break;
		default: Helpers::panic("AM service requested. Command: %08X\n", command);
	}
}

void AMService::listTitleInfo(u32 messagePointer) {
	log("AM::ListDLCOrLicenseTicketInfos\n");  // Yes this is the actual name
	u32 ticketCount = mem.read32(messagePointer + 4);
	u64 titleID = mem.read64(messagePointer + 8);
	u32 pointer = mem.read32(messagePointer + 24);

	for (u32 i = 0; i < ticketCount; i++) {
		mem.write64(pointer, titleID);  // Title ID
		mem.write64(pointer + 8, 0);    // Ticket ID
		mem.write16(pointer + 16, 0);   // Version
		mem.write16(pointer + 18, 0);   // Padding
		mem.write32(pointer + 20, 0);   // Size

		pointer += 24;  // = sizeof(TicketInfo)
	}

	mem.write32(messagePointer, IPC::responseHeader(0x1007, 2, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, ticketCount);
}

void AMService::getDLCTitleInfo(u32 messagePointer) {
	log("AM::GetDLCTitleInfo (stubbed to fail)\n");
	Helpers::warn("Unimplemented AM::GetDLCTitleInfo. Will need to be implemented to support DLC\n");

	mem.write32(messagePointer, IPC::responseHeader(0x1005, 1, 4));
	mem.write32(messagePointer + 4, Result::FailurePlaceholder);
}

void AMService::getPatchTitleInfo(u32 messagePointer) {
	log("AM::GetPatchTitleInfo (stubbed to fail)\n");
	Helpers::warn("Unimplemented AM::GetDLCTitleInfo. Will need to be implemented to support updates\n");

	mem.write32(messagePointer, IPC::responseHeader(0x100D, 1, 4));
	mem.write32(messagePointer + 4, Result::FailurePlaceholder);
}