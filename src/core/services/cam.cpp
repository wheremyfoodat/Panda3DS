#include "services/cam.hpp"
#include "ipc.hpp"

namespace CAMCommands {
	enum : u32 {
		DriverInitialize = 0x00390000,
		GetMaxLines = 0x000A0080
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
		case CAMCommands::GetMaxLines: getMaxLines(messagePointer); break;
		default: Helpers::panic("CAM service requested. Command: %08X\n", command);
	}
}

void CAMService::driverInitialize(u32 messagePointer) {
	log("CAM::DriverInitialize\n");
	mem.write32(messagePointer, IPC::responseHeader(0x39, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

// Algorithm taken from Citra
// https://github.com/citra-emu/citra/blob/master/src/core/hle/service/cam/cam.cpp#L465
void CAMService::getMaxLines(u32 messagePointer) {
	const u16 width = mem.read16(messagePointer + 4);
	const u16 height = mem.read16(messagePointer + 8);
	log("CAM::GetMaxLines (width = %d, height = %d)\n", width, height);

	constexpr u32 MIN_TRANSFER_UNIT = 256;
	constexpr u32 MAX_BUFFER_SIZE = 2560;
	if (width * height * 2 % MIN_TRANSFER_UNIT != 0) {
		Helpers::panic("CAM::GetMaxLines out of range");
	} else {
		u32 lines = MAX_BUFFER_SIZE / width;
		if (lines > height) {
			lines = height;
		}

		u32 result = Result::Success;
		while (height % lines != 0 || (lines * width * 2 % MIN_TRANSFER_UNIT != 0)) {
			--lines;
			if (lines == 0) {
				Helpers::panic("CAM::GetMaxLines out of range");
				break;
			}
		}

		mem.write32(messagePointer, IPC::responseHeader(0xA, 2, 0));
		mem.write32(messagePointer + 4, result);
		mem.write16(messagePointer + 8, lines);
	}
}