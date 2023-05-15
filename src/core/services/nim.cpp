#include "services/nim.hpp"
#include "ipc.hpp"

namespace NIMCommands {
	enum : u32 {
		Initialize = 0x00210000
	};
}

namespace Result {
	enum : u32 {
		Success = 0,
	};
}

void NIMService::reset() {}

void NIMService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case NIMCommands::Initialize: initialize(messagePointer); break;
		default: Helpers::panic("NIM service requested. Command: %08X\n", command);
	}
}

void NIMService::initialize(u32 messagePointer) {
	log("NIM::Initialize\n");
	mem.write32(messagePointer, IPC::responseHeader(0x21, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}