#include "services/fs.hpp"

namespace FSCommands {
	enum : u32 {
		Initialize = 0x08010002,
		OpenArchive = 0x080C00C2
	};
}

namespace Result {
	enum : u32 {
		Success = 0,
		Failure = 0xFFFFFFFF
	};
}

void FSService::reset() {}

void FSService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case FSCommands::Initialize: initialize(messagePointer); break;
		case FSCommands::OpenArchive: openArchive(messagePointer); break;
		default: Helpers::panic("FS service requested. Command: %08X\n", command);
	}
}

void FSService::initialize(u32 messagePointer) {
	log("FS::Initialize (failure)\n");
	mem.write32(messagePointer + 4, Result::Failure);
}

void FSService::openArchive(u32 messagePointer) {
	log("FS::OpenArchive (failure)\n");
	mem.write32(messagePointer + 4, Result::Failure);
}