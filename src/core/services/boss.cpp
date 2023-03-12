#include "services/boss.hpp"

namespace BOSSCommands {
	enum : u32 {
		InitializeSession = 0x00010082,
		GetOptoutFlag = 0x000A0000,
		GetTaskIdList = 0x000E0000
	};
}

namespace Result {
	enum : u32 {
		Success = 0,
	};
}

void BOSSService::reset() {
	optoutFlag = 0;
}

void BOSSService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case BOSSCommands::GetOptoutFlag: getOptoutFlag(messagePointer); break;
		case BOSSCommands::GetTaskIdList: getTaskIdList(messagePointer); break;
		case BOSSCommands::InitializeSession: initializeSession(messagePointer); break;
		default: Helpers::panic("BOSS service requested. Command: %08X\n", command);
	}
}

void BOSSService::initializeSession(u32 messagePointer) {
	log("BOSS::InitializeSession (stubbed)\n");
	mem.write32(messagePointer + 4, Result::Success);
}

void BOSSService::getOptoutFlag(u32 messagePointer) {
	log("BOSS::getOptoutFlag\n");
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, optoutFlag);
}

void BOSSService::getTaskIdList(u32 messagePointer) {
	log("BOSS::GetTaskIdList (stubbed)\n");
	mem.write32(messagePointer + 4, Result::Success);
}