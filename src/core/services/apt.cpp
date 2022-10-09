#include "services/apt.hpp"

namespace APTCommands {
	enum : u32 {
		GetLockHandle = 0x00010040,
		Enable = 0x00030040,
		CheckNew3DS = 0x01020000,
		NotifyToWait = 0x00430040
	};
}

namespace Model {
	enum : u8 {
		Old3DS = 0,
		New3DS = 1
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
		case APTCommands::CheckNew3DS: checkNew3DS(messagePointer); break;
		case APTCommands::Enable: enable(messagePointer); break;
		case APTCommands::GetLockHandle: getLockHandle(messagePointer); break;
		case APTCommands::NotifyToWait: notifyToWait(messagePointer); break;
		default: Helpers::panic("APT service requested. Command: %08X\n", command);
	}
}

void APTService::checkNew3DS(u32 messagePointer) {
	log("APT::CheckNew3DS\n");
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, Model::Old3DS); // u8, Status (0 = Old 3DS, 1 = New 3DS)
}

void APTService::enable(u32 messagePointer) {
	log("APT::Enable\n");
	mem.write32(messagePointer + 4, Result::Success);
}

void APTService::getLockHandle(u32 messagePointer) {
	log("APT::GetLockHandle (Failure)\n");
	mem.write32(messagePointer + 4, Result::Failure); // Result code
	mem.write32(messagePointer + 16, 0); // Translation descriptor
}

// This apparently does nothing on the original kernel either?
void APTService::notifyToWait(u32 messagePointer) {
	log("APT::NotifyToWat\n");
	mem.write32(messagePointer + 4, Result::Success);
}