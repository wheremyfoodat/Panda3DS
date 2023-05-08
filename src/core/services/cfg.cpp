#include "services/cfg.hpp"
#include "services/dsp.hpp"
#include "system_models.hpp"
#include "ipc.hpp"

#include <array>
#include <bit>

namespace CFGCommands {
	enum : u32 {
		GetConfigInfoBlk2 = 0x00010082,
		SecureInfoGetRegion = 0x00020000,
		GenHashConsoleUnique = 0x00030040,
		GetRegionCanadaUSA = 0x00040000,
		GetSystemModel = 0x00050000
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
		case CFGCommands::GetConfigInfoBlk2: [[likely]] getConfigInfoBlk2(messagePointer); break;
		case CFGCommands::GetRegionCanadaUSA: getRegionCanadaUSA(messagePointer); break;
		case CFGCommands::GetSystemModel: getSystemModel(messagePointer); break;
		case CFGCommands::GenHashConsoleUnique: genUniqueConsoleHash(messagePointer); break;
		case CFGCommands::SecureInfoGetRegion: secureInfoGetRegion(messagePointer); break;
		default: Helpers::panic("CFG service requested. Command: %08X\n", command);
	}
}

void CFGService::getSystemModel(u32 messagePointer) {
	log("CFG::GetSystemModel\n");

	mem.write32(messagePointer, IPC::responseHeader(0x05, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, SystemModel::Nintendo3DS); // TODO: Make this adjustable via GUI
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
		mem.write8(output + 3, static_cast<u8>(country)); // Country code
	} else if (size == 0x20 && blockID == 0x50005) {
		// "Stereo Camera settings"
		// Implementing this properly fixes NaN uniforms in some games. Values taken from 3dmoo & Citra
		static constexpr std::array<float, 8> STEREO_CAMERA_SETTINGS = {
			62.0f, 289.0f, 76.80000305175781f, 46.08000183105469f,
			10.0f, 5.0f,   55.58000183105469f, 21.56999969482422f
		};

		for (int i = 0; i < 8; i++) {
			mem.write32(output + i * 4, std::bit_cast<u32, float>(STEREO_CAMERA_SETTINGS[i]));
		}
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
	} else if (size == 2 && blockID == 0xA0001) { // Birthday
		mem.write8(output, 5); // Month (May)
		mem.write8(output + 1, 5); // Day (Fifth)
	} else if (size == 8 && blockID == 0x30001) { // User time offset
		printf("Read from user time offset field in NAND. TODO: What is this\n");
		mem.write64(output, 0);
	} else if (size == 20 && blockID == 0xC0001) { // COPPACS restriction data, used by games when they detect a USA/Canada region for market restriction stuff
		for (u32 i = 0; i < size; i += 4) {
			mem.write32(output + i, 0);
		}
	} else {
		Helpers::panic("Unhandled GetConfigInfoBlk2 configuration. Size = %d, block = %X", size, blockID);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
}

void CFGService::secureInfoGetRegion(u32 messagePointer) {
	log("CFG::SecureInfoGetRegion\n");

	mem.write32(messagePointer, IPC::responseHeader(0x2, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, static_cast<u32>(Regions::USA)); // TODO: Detect the game region and report it
}

void CFGService::genUniqueConsoleHash(u32 messagePointer) {
	log("CFG::GenUniqueConsoleHash (semi-stubbed)\n");
	const u32 salt = mem.read32(messagePointer + 4) & 0x000FFFFF;

	mem.write32(messagePointer, IPC::responseHeader(0x3, 3, 0));
	mem.write32(messagePointer + 4, Result::Success);
	// We need to implement hash generation & the SHA-256 digest properly later on. We have cryptopp so the hashing isn't too hard to do
	// Let's stub it for now
	mem.write32(messagePointer + 8, 0x33646D6F ^ salt); // Lower word of hash
	mem.write32(messagePointer + 12, 0xA3534841 ^ salt);  // Upper word of hash
}

// Returns 1 if the console region is either Canada or USA, otherwise returns 0
// Used for market restriction-related stuff
void CFGService::getRegionCanadaUSA(u32 messagePointer) {
	log("CFG::GetRegionCanadaUSA\n");
	const u8 ret = (country == CountryCodes::US || country == CountryCodes::CA) ? 1 : 0;

	mem.write32(messagePointer, IPC::responseHeader(0x4, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, ret);
}