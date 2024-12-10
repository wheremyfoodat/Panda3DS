#include "services/boss.hpp"

#include "ipc.hpp"

namespace BOSSCommands {
	enum : u32 {
		InitializeSession = 0x00010082,
		UnregisterStorage = 0x00030000,
		GetTaskStorageInfo = 0x00040000,
		GetNewArrivalFlag = 0x00070000,
		RegisterNewArrivalEvent = 0x00080002,
		SetOptoutFlag = 0x00090040,
		GetOptoutFlag = 0x000A0000,
		RegisterTask = 0x000B00C2,
		UnregisterTask = 0x000C0082,
		GetTaskIdList = 0x000E0000,
		GetNsDataIdList = 0x00100102,
		GetNsDataIdList1 = 0x00110102,
		GetNsDataIdList2 = 0x00120102,
		GetNsDataIdList3 = 0x00130102,
		SendProperty = 0x00140082,
		ReceiveProperty = 0x00160082,
		GetTaskServiceStatus = 0x001B0042,
		StartTask = 0x001C0042,
		CancelTask = 0x001E0042,
		GetTaskState = 0x00200082,
		GetTaskStatus = 0x002300C2,
		GetTaskInfo = 0x00250082,
		DeleteNsData = 0x00260040,
		GetNsDataHeaderInfo = 0x002700C2,
		ReadNsData = 0x00280102,
		GetNsDataLastUpdated = 0x002D0040,
		GetErrorCode = 0x002E0040,
		RegisterStorageEntry = 0x002F0140,
		GetStorageEntryInfo = 0x00300000,
		StartBgImmediate = 0x00330042,
		InitializeSessionPrivileged = 0x04010082,
		GetAppNewFlag = 0x04040080,
		SetAppNewFlag = 0x040500C0,  // Probably
	};
}

void BOSSService::reset() { optoutFlag = 0; }

void BOSSService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case BOSSCommands::CancelTask: cancelTask(messagePointer); break;
		case BOSSCommands::DeleteNsData: deleteNsData(messagePointer); break;
		case BOSSCommands::GetAppNewFlag: getAppNewFlag(messagePointer); break;
		case BOSSCommands::GetErrorCode: getErrorCode(messagePointer); break;
		case BOSSCommands::GetNsDataHeaderInfo: getNsDataHeaderInfo(messagePointer); break;
		case BOSSCommands::GetNewArrivalFlag: getNewArrivalFlag(messagePointer); break;
		case BOSSCommands::GetNsDataIdList:
		case BOSSCommands::GetNsDataIdList1:
		case BOSSCommands::GetNsDataIdList2:
		case BOSSCommands::GetNsDataIdList3: getNsDataIdList(messagePointer, command); break;
		case BOSSCommands::GetNsDataLastUpdated: getNsDataLastUpdated(messagePointer); break;
		case BOSSCommands::GetOptoutFlag: getOptoutFlag(messagePointer); break;
		case BOSSCommands::GetStorageEntryInfo: getStorageEntryInfo(messagePointer); break;
		case BOSSCommands::GetTaskIdList: getTaskIdList(messagePointer); break;
		case BOSSCommands::GetTaskInfo: getTaskInfo(messagePointer); break;
		case BOSSCommands::GetTaskServiceStatus: getTaskServiceStatus(messagePointer); break;
		case BOSSCommands::GetTaskState: getTaskState(messagePointer); break;
		case BOSSCommands::GetTaskStatus: getTaskStatus(messagePointer); break;
		case BOSSCommands::GetTaskStorageInfo: getTaskStorageInfo(messagePointer); break;
		case BOSSCommands::InitializeSession:
		case BOSSCommands::InitializeSessionPrivileged: initializeSession(messagePointer); break;
		case BOSSCommands::ReadNsData: readNsData(messagePointer); break;
		case BOSSCommands::ReceiveProperty: receiveProperty(messagePointer); break;
		case BOSSCommands::RegisterNewArrivalEvent: registerNewArrivalEvent(messagePointer); break;
		case BOSSCommands::RegisterStorageEntry: registerStorageEntry(messagePointer); break;
		case BOSSCommands::RegisterTask: registerTask(messagePointer); break;
		case BOSSCommands::SendProperty: sendProperty(messagePointer); break;
		case BOSSCommands::SetAppNewFlag: setAppNewFlag(messagePointer); break;
		case BOSSCommands::SetOptoutFlag: setOptoutFlag(messagePointer); break;
		case BOSSCommands::StartBgImmediate: startBgImmediate(messagePointer); break;
		case BOSSCommands::StartTask: startTask(messagePointer); break;
		case BOSSCommands::UnregisterStorage: unregisterStorage(messagePointer); break;
		case BOSSCommands::UnregisterTask: unregisterTask(messagePointer); break;

		case 0x04500102:  // Home Menu uses this command, what is this?
			Helpers::warn("BOSS command 0x04500102");
			mem.write32(messagePointer, IPC::responseHeader(0x450, 1, 0));
			mem.write32(messagePointer + 4, Result::Success);
			break;

		default:
			mem.write32(messagePointer + 4, Result::Success);
			Helpers::warn("BOSS service requested. Command: %08X\n", command);
			break;
	}
}

void BOSSService::initializeSession(u32 messagePointer) {
	log("BOSS::InitializeSession (stubbed)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void BOSSService::setOptoutFlag(u32 messagePointer) {
	const s8 flag = static_cast<s8>(mem.read8(messagePointer + 4));
	log("BOSS::SetOptoutFlag (flag = %d)\n", flag);
	optoutFlag = flag;

	mem.write32(messagePointer, IPC::responseHeader(0x9, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void BOSSService::getOptoutFlag(u32 messagePointer) {
	log("BOSS::GetOptoutFlag\n");
	mem.write32(messagePointer, IPC::responseHeader(0xA, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, optoutFlag);
}

void BOSSService::getTaskState(u32 messagePointer) {
	const u32 taskIDBufferSize = mem.read32(messagePointer + 4);
	const u32 taskIDDataPointer = mem.read32(messagePointer + 16);
	log("BOSS::GetTaskStatus (task buffer size: %08X, task data pointer: %08X) (stubbed)\n", taskIDBufferSize, taskIDDataPointer);

	mem.write32(messagePointer, IPC::responseHeader(0x20, 2, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, 0);    // TaskStatus: Report the task finished successfully
	mem.write32(messagePointer + 12, 0);  // Current state value for task PropertyID 0x4
	mem.write8(messagePointer + 16, 0);   // TODO: Figure out what this should be
}

void BOSSService::getTaskStatus(u32 messagePointer) {
	// TODO: 3DBrew does not mention what the parameters are, or what the return values are.
	log("BOSS::GetTaskStatus (Stubbed)\n");

	// Response values stubbed based on Citra
	mem.write32(messagePointer, IPC::responseHeader(0x23, 2, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, 0);
	// TODO: Citra pushes a buffer here?
}

void BOSSService::getTaskServiceStatus(u32 messagePointer) {
	// TODO: 3DBrew does not mention what the parameters are, or what the return values are... again
	log("BOSS::GetTaskServiceStatus (Stubbed)\n");

	// Response values stubbed based on Citra
	mem.write32(messagePointer, IPC::responseHeader(0x1B, 2, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, 0);
	// TODO: Citra pushes a buffer here too?
}

void BOSSService::getTaskStorageInfo(u32 messagePointer) {
	log("BOSS::GetTaskStorageInfo (stubbed)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x4, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);
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

void BOSSService::getErrorCode(u32 messagePointer) {
	log("BOSS::GetErrorCode (stubbed)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x2E, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, Result::Success);  // No error code
}

void BOSSService::getStorageEntryInfo(u32 messagePointer) {
	log("BOSS::GetStorageEntryInfo (undocumented)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x30, 3, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);   // u32, unknown meaning
	mem.write16(messagePointer + 12, 0);  // s16, unknown meaning
}

void BOSSService::sendProperty(u32 messagePointer) {
	const u32 id = mem.read32(messagePointer + 4);
	const u32 size = mem.read32(messagePointer + 8);
	const u32 ptr = mem.read32(messagePointer + 16);

	log("BOSS::SendProperty (id = %d, size = %08X, ptr = %08X) (stubbed)\n", id, size, ptr);
	mem.write32(messagePointer, IPC::responseHeader(0x14, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);  // Read size
	// TODO: Should this do anything else?
}

void BOSSService::receiveProperty(u32 messagePointer) {
	const u32 id = mem.read32(messagePointer + 4);
	const u32 size = mem.read32(messagePointer + 8);
	const u32 ptr = mem.read32(messagePointer + 16);

	log("BOSS::ReceiveProperty (id = %d, size = %08X, ptr = %08X) (stubbed)\n", id, size, ptr);
	mem.write32(messagePointer, IPC::responseHeader(0x16, 2, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);  // Read size
}

// This seems to accept a KEvent as a parameter and register it for something Spotpass related
// I need to update the 3DBrew page when it's known what it does properly
void BOSSService::registerNewArrivalEvent(u32 messagePointer) {
	const Handle eventHandle = mem.read32(messagePointer + 4);  // Kernel event handle to register
	log("BOSS::RegisterNewArrivalEvent (handle = %X)\n", eventHandle);

	mem.write32(messagePointer, IPC::responseHeader(0x8, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void BOSSService::startTask(u32 messagePointer) {
	log("BOSS::StartTask (stubbed)\n");
	const u32 bufferSize = mem.read32(messagePointer + 4);
	const u32 descriptor = mem.read32(messagePointer + 8);
	const u32 bufferData = mem.read32(messagePointer + 12);

	mem.write32(messagePointer, IPC::responseHeader(0x1C, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
}

void BOSSService::cancelTask(u32 messagePointer) {
	log("BOSS::CancelTask (stubbed)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x1E, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
}

void BOSSService::registerTask(u32 messagePointer) {
	log("BOSS::RegisterTask (stubbed)\n");
	const u32 bufferSize = mem.read32(messagePointer + 4);
	const u32 dataPointr = mem.read32(messagePointer + 20);

	mem.write32(messagePointer, IPC::responseHeader(0x0B, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
}

void BOSSService::unregisterTask(u32 messagePointer) {
	log("BOSS::UnregisterTask (stubbed)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x0C, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
}

// There's multiple aliases for this command. commandWord is the first word in the IPC buffer with the command word, needed for the response header
void BOSSService::getNsDataIdList(u32 messagePointer, u32 commandWord) {
	log("BOSS::GetNsDataIdList (stubbed)\n");

	mem.write32(messagePointer, IPC::responseHeader(commandWord >> 16, 3, 2));
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

void BOSSService::getNewArrivalFlag(u32 messagePointer) {
	log("BOSS::GetNewArrivalFlag (stubbed)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x7, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, 0);  // Flag
}

void BOSSService::startBgImmediate(u32 messagePointer) {
	const u32 size = mem.read32(messagePointer + 8);
	const u32 taskIDs = mem.read32(messagePointer + 12);
	log("BOSS::StartBgImmediate (size = %X, task ID pointer = %X) (stubbed)\n", size, taskIDs);

	mem.write32(messagePointer, IPC::responseHeader(0x33, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, IPC::pointerHeader(0, size, IPC::BufferType::Send));
	mem.write32(messagePointer + 12, taskIDs);
}

void BOSSService::getAppNewFlag(u32 messagePointer) {
	const u64 appID = mem.read64(messagePointer + 4);
	log("BOSS::GetAppNewFlag (app ID = %llX)\n", appID);

	mem.write32(messagePointer, IPC::responseHeader(0x404, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, 0);  // No new content
}

void BOSSService::getNsDataHeaderInfo(u32 messagePointer) {
	const u32 nsDataID = mem.read32(messagePointer + 4);
	const u8 type = mem.read8(messagePointer + 8);
	const u32 size = mem.read32(messagePointer + 12);
	const u32 nsDataHeaderInfo = mem.read32(messagePointer + 20);
	log("BOSS::GetNsDataHeaderInfo (NS data ID = %X, type = %X, size = %X, NS data header info pointer = %X) (stubbed)\n", nsDataID, type, size,
		nsDataHeaderInfo);

	switch (type) {
		case 3:
		case 5: mem.write32(nsDataHeaderInfo, 0); break;  // ??

		default: Helpers::panic("Unimplemented NS data header info type %X", type);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x27, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, IPC::pointerHeader(0, size, IPC::BufferType::Receive));
	mem.write32(messagePointer + 12, nsDataHeaderInfo);
}

void BOSSService::getNsDataLastUpdated(u32 messagePointer) {
	const u32 nsDataID = mem.read32(messagePointer + 4);
	log("BOSS::GetNsDataLastUpdated (NS data ID = %X) (stubbed)\n", nsDataID);

	mem.write32(messagePointer, IPC::responseHeader(0x2D, 3, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write64(messagePointer + 8, 0);  // Milliseconds since last update?
}

void BOSSService::readNsData(u32 messagePointer) {
	const u32 nsDataID = mem.read32(messagePointer + 4);
	const s64 offset = mem.read64(messagePointer + 8);
	const u32 size = mem.read32(messagePointer + 20);
	const u32 data = mem.read32(messagePointer + 24);
	log("BOSS::ReadNsData (NS data ID = %X, offset = %llX, size = %X, data pointer = %X) (stubbed)\n", nsDataID, offset, size, data);

	for (u32 i = 0; i < size; i++) {
		mem.write8(data + i, 0);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x28, 3, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, size);  // Technically how many bytes have been read
	mem.write32(messagePointer + 12, 0);    // ??
	mem.write32(messagePointer + 16, IPC::pointerHeader(0, size, IPC::BufferType::Receive));
	mem.write32(messagePointer + 20, data);
}

void BOSSService::deleteNsData(u32 messagePointer) {
	const u32 nsDataID = mem.read32(messagePointer + 4);
	log("BOSS::DeleteNsData (NS data ID = %X) (stubbed)\n", nsDataID);

	mem.write32(messagePointer, IPC::responseHeader(0x26, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

// Judging by the inputs and command number, this could very well be a "SetAppNewFlag"
void BOSSService::setAppNewFlag(u32 messagePointer) {
	const u64 appID = mem.read64(messagePointer + 4);
	const u8 flag = mem.read32(messagePointer + 12);
	log("BOSS::SetAppNewFlag (app ID = %llX, flag = %X)\n", appID, flag);

	mem.write32(messagePointer, IPC::responseHeader(0x405, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}