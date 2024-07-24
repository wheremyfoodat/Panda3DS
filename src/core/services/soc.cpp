#include "services/soc.hpp"

#include "ipc.hpp"
#include "result/result.hpp"

namespace SOCCommands {
	enum : u32 {
		InitializeSockets = 0x00010044,
	};
}

void SOCService::reset() { initialized = false; }

void SOCService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case SOCCommands::InitializeSockets: initializeSockets(messagePointer); break;
		default: Helpers::panic("SOC service requested. Command: %08X\n", command);
	}
}

void SOCService::initializeSockets(u32 messagePointer) {
	const u32 memoryBlockSize = mem.read32(messagePointer + 4);
	const HandleType sharedMemHandle = mem.read32(messagePointer + 20);
	log("SOC::InitializeSockets (memory block size = %08X, shared mem handle = %08X)\n", memoryBlockSize, sharedMemHandle);

	// TODO: Does double initialization return an error code?
	// TODO: Implement the rest of this stuff when it's time to do online. Also implement error checking for the size, shared mem handle, and so on
	initialized = true;

	mem.write32(messagePointer, IPC::responseHeader(0x01, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}
