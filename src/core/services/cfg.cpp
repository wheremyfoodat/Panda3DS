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

// Write a UTF16 string to 3DS memory starting at "pointer". Appends a null terminator.
void CFGService::writeStringU16(u32 pointer, const std::u16string& string) {
	for (auto c : string) {
		mem.write16(pointer, static_cast<u16>(c));
		pointer += 2;
	}

	mem.write16(pointer, static_cast<u16>(u'\0')); // Null terminator
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
	} else if (size == 4 && blockID == 0xB0000) { // Country info
		mem.write8(output, 0); // Unknown
		mem.write8(output + 1, 0); // Unknown
		mem.write8(output + 2, 2); // Province (Temporarily stubbed to Washington DC like Citra)
		mem.write8(output + 3, 49); // Country code (Temporarily stubbed to USA like Citra)
	} else if (size == 0x20 && blockID == 0x50005) {
		printf("[Unimplemented] Read stereo display settings from NAND\n");
	} else if (size == 0x1C && blockID == 0xA0000) { // Username
		writeStringU16(output, u"Pander");
	} else if (size == 0xC0 && blockID == 0xC0000) { // Parental restrictions info
		for (int i = 0; i < 0xC0; i++)
			mem.write8(output + i, 0);
	} else if (size == 4 && blockID == 0xD0000) { // Agreed EULA version (first 2 bytes) and latest EULA version (next 2 bytes)
		log("Read EULA info\n");
		mem.write16(output, 0x0202); // Agreed EULA version = 2.2 (Random number. TODO: Check)
		mem.write16(output + 2, 0x0202); // Latest EULA version = 2.2 
	} else if (size == 0x800 && blockID == 0xB0001) { // UTF-16 name for our country in every language at 0x80 byte intervals
		constexpr size_t languageCount = 16;
		constexpr size_t nameSize = 0x80; // Max size of each name in bytes
		std::u16string name = u"PandaLand (Home of PandaSemi LLC) (aka Pandistan)"; // Note: This + the null terminator needs to fit in 0x80 bytes

		for (int i = 0; i < languageCount; i++) {
			u32 pointer = output + i * nameSize;
			writeStringU16(pointer, name);
		}
	} else if (size == 0x800 && blockID == 0xB0002) { // UTF-16 name for our state in every language at 0x80 byte intervals
		constexpr size_t languageCount = 16;
		constexpr size_t nameSize = 0x80; // Max size of each name in bytes
		std::u16string name = u"Pandington"; // Note: This + the null terminator needs to fit in 0x80 bytes

		for (int i = 0; i < languageCount; i++) {
			u32 pointer = output + i * nameSize; 
			writeStringU16(pointer, name);
		}
	} else if (size == 4 && blockID == 0xB0003) { // Coordinates (latidude and longtitude) as s16
		mem.write16(output, 0); // Latitude
		mem.write16(output + 2, 0); // Longtitude
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