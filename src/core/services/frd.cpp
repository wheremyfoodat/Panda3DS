#include "services/frd.hpp"

namespace FRDCommands {
	enum : u32 {
		SetClientSdkVersion = 0x00320042,
		GetMyFriendKey = 0x00050000,
		GetMyPresence = 0x00080000
	};
}

namespace Result {
	enum : u32 {
		Success = 0,
	};
}

void FRDService::reset() {}

void FRDService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case FRDCommands::GetMyFriendKey: getMyFriendKey(messagePointer); break;
		case FRDCommands::GetMyPresence: getMyPresence(messagePointer); break;
		case FRDCommands::SetClientSdkVersion: setClientSDKVersion(messagePointer); break;
		default: Helpers::panic("FRD service requested. Command: %08X\n", command);
	}
}

void FRDService::getMyFriendKey(u32 messagePointer) {
	log("FRD::GetMyFriendKey");

	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);  // Principal ID
	mem.write32(messagePointer + 12, 0); // Padding (?)
	mem.write32(messagePointer + 16, 0); // Local friend code
	mem.write32(messagePointer + 20, 0);
}

void FRDService::getMyPresence(u32 messagePointer) {
	static constexpr u32 presenceSize = 0x12C; // A presence seems to be 12C bytes of data, not sure what it contains
	log("FRD::GetMyPresence\n");
	u32 buffer = mem.read32(messagePointer + 0x104); // Buffer to write presence info to.

	for (u32 i = 0; i < presenceSize; i += 4) { // Clear presence info with 0s for now
		mem.write32(buffer + i, 0);
	}

	mem.write32(messagePointer + 4, Result::Success);
}

void FRDService::setClientSDKVersion(u32 messagePointer) {
	u32 version = mem.read32(messagePointer + 4);
	log("FRD::SetClientSdkVersion (version = %d)\n", version);

	mem.write32(messagePointer + 4, Result::Success);
}