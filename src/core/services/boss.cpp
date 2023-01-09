#include "services/boss.hpp"

namespace BOSSCommands {
	enum : u32 {
		InitializeSession = 0x00010082
	};
}

namespace Result {
	enum : u32 {
		Success = 0,
	};
}

void BOSSService::reset() {}

void BOSSService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case BOSSCommands::InitializeSession: initializeSession(messagePointer); break;
		default: Helpers::panic("BOSS service requested. Command: %08X\n", command);
	}
}

void BOSSService::initializeSession(u32 messagePointer) {
	log("BOSS::InitializeSession (stubbed)\n");
	mem.write32(messagePointer + 4, Result::Success);
}