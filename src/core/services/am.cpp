#include "services/am.hpp"
#include "ipc.hpp"

namespace AMCommands {
	enum : u32 {
		GetProgramInfos = 0x00030084,
		NeedsCleanup = 0x00130040,
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
		case AMCommands::GetProgramInfos: getProgramInfos(messagePointer); break;
		case AMCommands::ListTitleInfo: listTitleInfo(messagePointer); break;
		case AMCommands::NeedsCleanup: needsCleanup(messagePointer); break;
		default: Helpers::panic("AM service requested. Command: %08X\n", command);
	}
}

void AMService::listTitleInfo(u32 messagePointer) {
	log("AM::ListDLCOrLicenseTicketInfos\n"); // Yes this is the actual name
	u32 ticketCount = mem.read32(messagePointer + 4);
	u64 titleID = mem.read64(messagePointer + 8);
	u32 pointer = mem.read32(messagePointer + 24);

	for (u32 i = 0; i < ticketCount; i++) {
		mem.write64(pointer, titleID); // Title ID
		mem.write64(pointer + 8, 0);   // Ticket ID
		mem.write16(pointer + 16, 0);  // Version
		mem.write16(pointer + 18, 0);  // Padding
		mem.write32(pointer + 20, 0);  // Size

		pointer += 24; // = sizeof(TicketInfo)
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

void AMService::needsCleanup(u32 messagePointer) {
	const u32 mediaType = mem.read32(messagePointer + 4);
	log("AM::NeedsCleanup (media type = %X)\n", mediaType);

	mem.write32(messagePointer, IPC::responseHeader(0x13, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, 0); // Doesn't need cleanup
}

void AMService::getProgramInfos(u32 messagePointer) {
	const u8 mediaType = mem.read8(messagePointer + 4);
	const u32 titleCount = mem.read32(messagePointer + 8);
	const u32 titleIDs = mem.read32(messagePointer + 16);
	const u32 titleInfos = mem.read32(messagePointer + 24);
	log("AM::GetProgramInfos (media type = %X, title count = %X, title IDs pointer = %X, title infos pointer = %X) (Stubbed)\n", mediaType, titleCount, titleIDs, titleInfos);

	mem.write32(messagePointer, IPC::responseHeader(0x3, 1, 4));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, IPC::pointerHeader(0, sizeof(u64) * titleCount, IPC::BufferType::Send));
	mem.write32(messagePointer + 12, 0xC0DEC0DE);
	mem.write32(messagePointer + 16, IPC::pointerHeader(1, sizeof(u32) * titleCount, IPC::BufferType::Receive));
	mem.write32(messagePointer + 20, 0xC0DEC0DE);
}