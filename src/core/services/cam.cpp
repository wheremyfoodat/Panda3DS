#include "services/cam.hpp"

namespace CAMCommands {
	enum : u32 {
		DriverInitialize = 0x00390000
	};
}

namespace Result {
	enum : u32 {
		Success = 0,
	};
}

void CAMService::reset() {}

void CAMService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case CAMCommands::DriverInitialize: driverInitialize(messagePointer); break;
		default: Helpers::panic("CAM service requested. Command: %08X\n", command);
	}
}

void CAMService::driverInitialize(u32 messagePointer) {
	log("CAM::DriverInitialize\n");
	mem.write32(messagePointer + 4, Result::Success);
}