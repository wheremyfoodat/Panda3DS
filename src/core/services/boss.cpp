#include "services/boss.hpp"
#include "ipc.hpp"

namespace BOSSCommands {
	enum : u32 {
		InitializeSession = 0x00010082,
		UnregisterStorage = 0x00030000,
		GetTaskStorageInfo = 0x00040000,
		GetOptoutFlag = 0x000A0000,
		UnregisterTask = 0x000C0082,
		GetTaskIdList = 0x000E0000,
		GetNsDataIdList = 0x00100102,
		ReceiveProperty = 0x00160082,
		GetTaskInfo = 0x00250082,
		RegisterStorageEntry = 0x002F0140,
		GetStorageEntryInfo = 0x00300000
	};
}

void BOSSService::reset() {
	optoutFlag = 0;
}

void BOSSService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case BOSSCommands::GetNsDataIdList: getNsDataIdList(messagePointer); break;
		case BOSSCommands::GetOptoutFlag: getOptoutFlag(messagePointer); break;
		case BOSSCommands::GetStorageEntryInfo: getStorageEntryInfo(messagePointer); break;
		case BOSSCommands::GetTaskIdList: getTaskIdList(messagePointer); break;
		case BOSSCommands::GetTaskInfo: getTaskInfo(messagePointer); break;
		case BOSSCommands::GetTaskStorageInfo: getTaskStorageInfo(messagePointer); break;
		case BOSSCommands::InitializeSession: initializeSession(messagePointer); break;
		case BOSSCommands::ReceiveProperty: receiveProperty(messagePointer); break;
		case BOSSCommands::RegisterStorageEntry: registerStorageEntry(messagePointer); break;
		case BOSSCommands::UnregisterStorage: unregisterStorage(messagePointer); break;
		case BOSSCommands::UnregisterTask: unregisterTask(messagePointer); break;
		default: Helpers::panic("BOSS service requested. Command: %08X\n", command);
	}
}

void BOSSService::initializeSession(u32 messagePointer) {
	log("BOSS::InitializeSession (stubbed)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void BOSSService::getOptoutFlag(u32 messagePointer) {
	log("BOSS::getOptoutFlag\n");
	mem.write32(messagePointer, IPC::responseHeader(0xA, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, optoutFlag);
}

void BOSSService::getTaskStorageInfo(u32 messagePointer) {
	log("BOSS::GetTaskStorageInfo (stubbed)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x4, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);  // Seems to be unknown what this is?
}

void BOSSService::getTaskIdList(u32 messagePointer) {
	log("BOSS::GetTaskIdList (stubbed)\n");
	mem.write32(messagePointer, IPC::responseHeader(0xE, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

// This function is completely undocumented, including on 3DBrew
// The name GetTaskInfo is taken from Citra source and nobody seems to know what exactly it does
// Kid Icarus: Uprising uses it on startup
void BOSSService::getTaskInfo(u32 messagePointer) {
	log("BOSS::GetTaskInfo (stubbed and undocumented)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x25, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
}

void BOSSService::getStorageEntryInfo(u32 messagePointer) {
	log("BOSS::GetStorageEntryInfo (undocumented)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x30, 3, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0); // u32, unknown meaning
	mem.write16(messagePointer + 12, 0); // s16, unknown meaning
}

void BOSSService::receiveProperty(u32 messagePointer) {
	const u32 id = mem.read32(messagePointer + 4);
	const u32 size = mem.read32(messagePointer + 8);
	const u32 ptr = mem.read32(messagePointer + 16);

	log("BOSS::ReceiveProperty(stubbed) (id = %d, size = %08X, ptr = %08X)\n", id, size, ptr);
	mem.write32(messagePointer, IPC::responseHeader(0x16, 2, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0); // Read size
}

void BOSSService::unregisterTask(u32 messagePointer) {
	log("BOSS::UnregisterTask (stubbed)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x0C, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
}

void BOSSService::getNsDataIdList(u32 messagePointer) {
	log("BOSS::GetNsDataIdList (stubbed)\n");

	mem.write32(messagePointer, IPC::responseHeader(0x10, 3, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write16(messagePointer + 8, 0);   // u16: Actual number of output entries.
	mem.write16(messagePointer + 12, 0);  // u16: Last word-index copied to output in the internal NsDataId list.
}

void BOSSService::registerStorageEntry(u32 messagePointer) {
	log("BOSS::RegisterStorageEntry (stubbed)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x2F, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void BOSSService::unregisterStorage(u32 messagePointer) {
	log("BOSS::UnregisterStorage (stubbed)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x3, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}