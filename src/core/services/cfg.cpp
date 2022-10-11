#include "services/cfg.hpp"
#include "services/dsp.hpp"

namespace CFGCommands {
	enum : u32 {
		GetConfigInfoBlk2 = 0x00010082
	};
}

namespace Result {
	enum : u32 {
		Success = 0,
	};
}

void CFGService::reset() {}

void CFGService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case CFGCommands::GetConfigInfoBlk2: getConfigInfoBlk2(messagePointer); break;
		default: Helpers::panic("CFG service requested. Command: %08X\n", command);
	}
}

void CFGService::getConfigInfoBlk2(u32 messagePointer) {
	u32 size = mem.read32(messagePointer + 4);
	u32 blockID = mem.read32(messagePointer + 8);
	u32 output = mem.read32(messagePointer + 16); // Pointer to write the output data to
	log("CFG::GetConfigInfoBlk2 (size = %X, block ID = %X, output pointer = %08X\n", size, blockID, output);

	// Implement checking the sound output mode
	if (size == 1 && blockID == 0x70001) {
		mem.write8(output, (u8) DSPService::SoundOutputMode::Stereo);
	} else {
		Helpers::panic("Unhandled GetConfigInfoBlk2 configuration");
	}

	mem.write32(messagePointer + 4, Result::Success);
}