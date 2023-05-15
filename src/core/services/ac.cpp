#include "services/ac.hpp"
#include "ipc.hpp"

namespace ACCommands {
	enum : u32 {
		SetClientVersion = 0x00400042
	};
}

namespace Result {
	enum : u32 {
		Success = 0,
	};
}

void ACService::reset() {}

void ACService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case ACCommands::SetClientVersion: setClientVersion(messagePointer); break;
		default: Helpers::panic("AC service requested. Command: %08X\n", command);
	}
}

void ACService::setClientVersion(u32 messagePointer) {
	u32 version = mem.read32(messagePointer + 4);
	log("AC::SetClientVersion (version = %d)\n", version);

	mem.write32(messagePointer, IPC::responseHeader(0x40, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}