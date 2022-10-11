#include "services/cfg.hpp"
#include "services/dsp.hpp"
#include "services/region_codes.hpp"

namespace CFGCommands {
	enum : u32 {
		GetConfigInfoBlk2 = 0x00010082,
		SecureInfoGetRegion = 0x00020000
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
		case CFGCommands::SecureInfoGetRegion: secureInfoGetRegion(messagePointer); break;
		default: Helpers::panic("CFG service requested. Command: %08X\n", command);
	}
}

void CFGService::getConfigInfoBlk2(u32 messagePointer) {
	u32 size = mem.read32(messagePointer + 4);
	u32 blockID = mem.read32(messagePointer + 8);
	u32 output = mem.read32(messagePointer + 16); // Pointer to write the output data to
	log("CFG::GetConfigInfoBlk2 (size = %X, block ID = %X, output pointer = %08X\n", size, blockID, output);

	// TODO: Make this not bad
	if (size == 1 && blockID == 0x70001) { // Sound output mode
		mem.write8(output, static_cast<u8>(DSPService::SoundOutputMode::Stereo));
	} else if (size == 1 && blockID == 0xA0002){ // System language
		mem.write8(output, static_cast<u8>(LanguageCodes::English));
	} else if (size == 0x20 && blockID == 0x50005) {
		printf("[Unimplemented] Read stereo display settings from NAND\n");
	} else {
		Helpers::panic("Unhandled GetConfigInfoBlk2 configuration");
	}

	mem.write32(messagePointer + 4, Result::Success);
}

void CFGService::secureInfoGetRegion(u32 messagePointer) {
	log("CFG::SecureInfoGetRegion\n");

	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, static_cast<u32>(Regions::USA)); // TODO: Detect the game region and report it
}