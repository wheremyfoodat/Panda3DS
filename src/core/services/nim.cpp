#include "services/nim.hpp"
#include "ipc.hpp"

namespace NIMCommands {
	enum : u32 {
		Initialize = 0x00210000
	};
}

void NIMService::reset() {}

void NIMService::handleSyncRequest(u32 messagePointer, Type type) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		if (type == Type::AOC) {
			switch (command) {
				case NIMCommands::Initialize: initialize(messagePointer); break;

				default: Helpers::panic("NIM AOC service requested. Command: %08X\n", command);
			}
		}
		default: Helpers::panic("NIM service requested. Command: %08X\n", command);
	}
}

void NIMService::initialize(u32 messagePointer) {
	log("NIM::Initialize\n");
	mem.write32(messagePointer, IPC::responseHeader(0x21, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}