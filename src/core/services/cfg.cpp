#include "services/cfg.hpp"
#include "services/dsp.hpp"
#include "system_models.hpp"
#include "ipc.hpp"

#include <array>
#include <bit>
#include <string>
#include <unordered_map>

namespace CFGCommands {
	enum : u32 {
		GetConfigInfoBlk2 = 0x00010082,
		GetConfigInfoBlk8 = 0x04010082,
		SecureInfoGetRegion = 0x00020000,
		GenHashConsoleUnique = 0x00030040,
		GetRegionCanadaUSA = 0x00040000,
		GetSystemModel = 0x00050000,
		TranslateCountryInfo = 0x00080080,
		GetCountryCodeID = 0x000A0040,
		SetConfigInfoBlk4 = 0x04020082,
		UpdateConfigNANDSavegame = 0x04030000,

		GetCountryCodeString = 0x00090040,
		GetCountryCodeID = 0x000A0040,
		IsFangateSupported = 0x000B0000,
		SetConfigInfoBlk4 = 0x04020082,
		UpdateConfigNANDSavegame = 0x04030000,
		GetLocalFriendCodeSeed = 0x04050000,
		SecureInfoGetByte101 = 0x04070000,
	};
}

// cfg:i commands
namespace CFGICommands {
	enum : u32 {
		GetConfigInfoBlk8 = 0x08010082,
	};
}

// cfg:nor commands
namespace NORCommands {
	enum : u32 {
		Initialize = 0x00010040,
		ReadData = 0x00050082,
	};
}

void CFGService::reset() {}

void CFGService::handleSyncRequest(u32 messagePointer, CFGService::Type type) {
	const u32 command = mem.read32(messagePointer);
	if (type != Type::NOR) {
		switch (command) {
			case CFGCommands::GetConfigInfoBlk2: [[likely]] getConfigInfoBlk2(messagePointer); break;
			case CFGCommands::GetCountryCodeString: getCountryCodeString(messagePointer); break;
			case CFGCommands::GetCountryCodeID: getCountryCodeID(messagePointer); break;
			case CFGCommands::GetRegionCanadaUSA: getRegionCanadaUSA(messagePointer); break;
			case CFGCommands::GetSystemModel: getSystemModel(messagePointer); break;
			case CFGCommands::GenHashConsoleUnique: genUniqueConsoleHash(messagePointer); break;
			case CFGCommands::IsFangateSupported: isFangateSupported(messagePointer); break;
			case CFGCommands::SecureInfoGetRegion: secureInfoGetRegion(messagePointer); break;
			case CFGCommands::TranslateCountryInfo: translateCountryInfo(messagePointer); break;

			default:
				if (type == Type::S) {
					// cfg:s (and cfg:i) functions only functions
					switch (command) {
						case CFGCommands::GetConfigInfoBlk8: getConfigInfoBlk8(messagePointer, command); break;
						case CFGCommands::GetLocalFriendCodeSeed: getLocalFriendCodeSeed(messagePointer); break;
						case CFGCommands::SecureInfoGetByte101: secureInfoGetByte101(messagePointer); break;
						case CFGCommands::SetConfigInfoBlk4: setConfigInfoBlk4(messagePointer); break;
						case CFGCommands::UpdateConfigNANDSavegame: updateConfigNANDSavegame(messagePointer); break;

						default: Helpers::panic("CFG:S service requested. Command: %08X\n", command);
					}
				} else if (type == Type::I) {
					switch (command) {
						case CFGCommands::GetConfigInfoBlk8:
						case CFGICommands::GetConfigInfoBlk8: getConfigInfoBlk8(messagePointer, command); break;

						default: Helpers::panic("CFG:I service requested. Command: %08X\n", command);
					}
				} else {
					Helpers::panic("CFG service requested. Command: %08X\n", command);
				}

				break;
		}
	} else {
		// cfg:nor functions
		switch (command) {
			case NORCommands::Initialize: norInitialize(messagePointer); break;
			case NORCommands::ReadData: norReadData(messagePointer); break;

			default: Helpers::panic("CFG:NOR service requested. Command: %08X\n", command);
		}
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
	log("CFG::GetConfigInfoBlk2 (size = %X, block ID = %X, output pointer = %08X)\n", size, blockID, output);

	getConfigInfo(output, blockID, size, 0x2);
	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
}

void CFGService::getConfigInfoBlk8(u32 messagePointer, u32 commandWord) {
	u32 size = mem.read32(messagePointer + 4);
	u32 blockID = mem.read32(messagePointer + 8);
	u32 output = mem.read32(messagePointer + 16);  // Pointer to write the output data to
	log("CFG::GetConfigInfoBlk8 (size = %X, block ID = %X, output pointer = %08X)\n", size, blockID, output);

	getConfigInfo(output, blockID, size, 0x8);
	mem.write32(messagePointer, IPC::responseHeader(commandWord >> 16, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
}

void CFGService::getConfigInfo(u32 output, u32 blockID, u32 size, u32 permissionMask) {
	// TODO: Make this not bad
	if (size == 1 && blockID == 0x70001) {  // Sound output mode
		mem.write8(output, static_cast<u8>(DSPService::SoundOutputMode::Stereo));
	} else if (size == 1 && blockID == 0xA0002) {  // System language
		mem.write8(output, static_cast<u8>(settings.systemLanguage));
	} else if (size == 4 && blockID == 0xB0000) {          // Country info
		mem.write8(output, 0);                             // Unknown
		mem.write8(output + 1, 0);                         // Unknown
		mem.write8(output + 2, 2);                         // Province (Temporarily stubbed to Washington DC like Citra)
		mem.write8(output + 3, static_cast<u8>(country));  // Country code
	} else if (size == 0x20 && blockID == 0x50005) {
		// "Stereo Camera settings"
		// Implementing this properly fixes NaN uniforms in some games. Values taken from 3dmoo & Citra
		static constexpr std::array<float, 8> STEREO_CAMERA_SETTINGS = {
			62.0f, 289.0f, 76.80000305175781f, 46.08000183105469f, 10.0f, 5.0f, 55.58000183105469f, 21.56999969482422f,
		};

		for (int i = 0; i < 8; i++) {
			mem.write32(output + i * 4, Helpers::bit_cast<u32, float>(STEREO_CAMERA_SETTINGS[i]));
		}
	} else if (size == 0x1C && blockID == 0xA0000) {  // Username
		writeStringU16(output, u"Pander");
	} else if (size == 0xC0 && blockID == 0xC0000) {  // Parental restrictions info
		for (int i = 0; i < 0xC0; i++) mem.write8(output + i, 0);
	} else if (size == 4 && blockID == 0xD0000) {  // Agreed EULA version (first 2 bytes) and latest EULA version (next 2 bytes)
		log("Read EULA info\n");
		mem.write16(output, 0x0202);                   // Agreed EULA version = 2.2 (Random number. TODO: Check)
		mem.write16(output + 2, 0x0202);               // Latest EULA version = 2.2
	} else if (size == 0x800 && blockID == 0xB0001) {  // UTF-16 name for our country in every language at 0x80 byte intervals
		constexpr size_t languageCount = 16;
		constexpr size_t nameSize = 0x80;                                            // Max size of each name in bytes
		std::u16string name = u"PandaLand (Home of PandaSemi LLC) (aka Pandistan)";  // Note: This + the null terminator needs to fit in 0x80 bytes

		for (int i = 0; i < languageCount; i++) {
			u32 pointer = output + i * nameSize;
			writeStringU16(pointer, name);
		}
	} else if (size == 0x800 && blockID == 0xB0002) {  // UTF-16 name for our state in every language at 0x80 byte intervals
		constexpr size_t languageCount = 16;
		constexpr size_t nameSize = 0x80;     // Max size of each name in bytes
		std::u16string name = u"Pandington";  // Note: This + the null terminator needs to fit in 0x80 bytes

		for (int i = 0; i < languageCount; i++) {
			u32 pointer = output + i * nameSize;
			writeStringU16(pointer, name);
		}
	} else if (size == 4 && blockID == 0xB0003) {  // Coordinates (latidude and longtitude) as s16
		mem.write16(output, 0);                    // Latitude
		mem.write16(output + 2, 0);                // Longtitude
	} else if (size == 2 && blockID == 0xA0001) {  // Birthday
		mem.write8(output, 5);                     // Month (May)
		mem.write8(output + 1, 5);                 // Day (Fifth)
	} else if (size == 8 && blockID == 0x30001) {  // User time offset
		printf("Read from user time offset field in NAND. TODO: What is this\n");
		mem.write64(output, 0);
	} else if (size == 20 && blockID == 0xC0001) {  // COPPACS restriction data, used by games when they detect a USA/Canada region for market
													// restriction stuff
		for (u32 i = 0; i < size; i += 4) {
			mem.write32(output + i, 0);
		}
	} else if (size == 4 && blockID == 0x170000) {  // Miiverse access key
		mem.write32(output, 0);
	} else if (size == 8 && blockID == 0x00090000) {
		mem.write64(output, 0);  // Some sort of key used with nwm::UDS::InitializeWithVersion
	} else if (size == 4 && blockID == 0x110000) {
		mem.write32(output, 1);  // According to 3Dbrew, 0 means system setup is required
	} else if (size == 2 && blockID == 0x50001) {
		// Backlight controls. Values taken from Citra
		mem.write8(output, 0);
		mem.write8(output + 1, 2);
	} else if (size == 8 && blockID == 0x50009) {
		// N3DS Backlight controls?
		mem.write64(output, 0);
	} else if (size == 4 && blockID == 0x180000) {
		// Infrared LED related?
		mem.write32(output, 0);
	} else if (size == 1 && blockID == 0xE0000) {
		mem.write8(output, 0);
	} else if ((size == 512 && blockID == 0xC0002) || (size == 148 && blockID == 0x100001)) {
		// CTR parental controls block (0xC0002) and TWL parental controls block (0x100001)
		for (u32 i = 0; i < size; i++) {
			mem.write8(output + i, 0);
		}
	} else if (size == 2 && blockID == 0x100000) {
		// EULA agreed
		mem.write8(output, 1);      // We have agreed to the EULA
		mem.write8(output + 1, 1);  // EULA version = 1
	} else if (size == 1 && blockID == 0x100002) {
		Helpers::warn("Unimplemented TWL country code access");
		mem.write8(output, 0);
	} else if (size == 24 && blockID == 0x180001) {
		// QTM calibration data
		for (u32 i = 0; i < size; i++) {
			mem.write8(output + i, 0);
		}
	} else {
		Helpers::panic("Unhandled GetConfigInfoBlk2 configuration. Size = %d, block = %X", size, blockID);
	}
}

void CFGService::secureInfoGetRegion(u32 messagePointer) {
	log("CFG::SecureInfoGetRegion\n");

	mem.write32(messagePointer, IPC::responseHeader(0x2, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, static_cast<u32>(mem.getConsoleRegion()));
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
	bool regionUSA = mem.getConsoleRegion() == Regions::USA;
	u8 ret;

	// First, this function checks that the console region is 1 (USA). If not then it instantly returns 0
	// Then it checks whether the country is US or Canda. If yes it returns 1, else it returns 0.
	if (!regionUSA) {
		ret = 0;
	} else {
		ret = (country == CountryCodes::US || country == CountryCodes::CA) ? 1 : 0;
	}

	mem.write32(messagePointer, IPC::responseHeader(0x4, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, ret);
}

constexpr u16 C(const char name[3]) { return name[0] | (name[1] << 8); }
static std::unordered_map<u16, u16> countryCodeToTableIDMap = {
	{C("JP"), 1},   {C("AI"), 8},   {C("AG"), 9},   {C("AR"), 10},  {C("AW"), 11},  {C("BS"), 12},  {C("BB"), 13},  {C("BZ"), 14},  {C("BO"), 15},
	{C("BR"), 16},  {C("VG"), 17},  {C("CA"), 18},  {C("KY"), 19},  {C("CL"), 20},  {C("CO"), 21},  {C("CR"), 22},  {C("DM"), 23},  {C("DO"), 24},
	{C("EC"), 25},  {C("SV"), 26},  {C("GF"), 27},  {C("GD"), 28},  {C("GP"), 29},  {C("GT"), 30},  {C("GY"), 31},  {C("HT"), 32},  {C("HN"), 33},
	{C("JM"), 34},  {C("MQ"), 35},  {C("MX"), 36},  {C("MS"), 37},  {C("AN"), 38},  {C("NI"), 39},  {C("PA"), 40},  {C("PY"), 41},  {C("PE"), 42},
	{C("KN"), 43},  {C("LC"), 44},  {C("VC"), 45},  {C("SR"), 46},  {C("TT"), 47},  {C("TC"), 48},  {C("US"), 49},  {C("UY"), 50},  {C("VI"), 51},
	{C("VE"), 52},  {C("AL"), 64},  {C("AU"), 65},  {C("AT"), 66},  {C("BE"), 67},  {C("BA"), 68},  {C("BW"), 69},  {C("BG"), 70},  {C("HR"), 71},
	{C("CY"), 72},  {C("CZ"), 73},  {C("DK"), 74},  {C("EE"), 75},  {C("FI"), 76},  {C("FR"), 77},  {C("DE"), 78},  {C("GR"), 79},  {C("HU"), 80},
	{C("IS"), 81},  {C("IE"), 82},  {C("IT"), 83},  {C("LV"), 84},  {C("LS"), 85},  {C("LI"), 86},  {C("LT"), 87},  {C("LU"), 88},  {C("MK"), 89},
	{C("MT"), 90},  {C("ME"), 91},  {C("MZ"), 92},  {C("NA"), 93},  {C("NL"), 94},  {C("NZ"), 95},  {C("NO"), 96},  {C("PL"), 97},  {C("PT"), 98},
	{C("RO"), 99},  {C("RU"), 100}, {C("RS"), 101}, {C("SK"), 102}, {C("SI"), 103}, {C("ZA"), 104}, {C("ES"), 105}, {C("SZ"), 106}, {C("SE"), 107},
	{C("CH"), 108}, {C("TR"), 109}, {C("GB"), 110}, {C("ZM"), 111}, {C("ZW"), 112}, {C("AZ"), 113}, {C("MR"), 114}, {C("ML"), 115}, {C("NE"), 116},
	{C("TD"), 117}, {C("SD"), 118}, {C("ER"), 119}, {C("DJ"), 120}, {C("SO"), 121}, {C("AD"), 122}, {C("GI"), 123}, {C("GG"), 124}, {C("IM"), 125},
	{C("JE"), 126}, {C("MC"), 127}, {C("TW"), 128}, {C("KR"), 136}, {C("HK"), 144}, {C("MO"), 145}, {C("ID"), 152}, {C("SG"), 153}, {C("TH"), 154},
	{C("PH"), 155}, {C("MY"), 156}, {C("CN"), 160}, {C("AE"), 168}, {C("IN"), 169}, {C("EG"), 170}, {C("OM"), 171}, {C("QA"), 172}, {C("KW"), 173},
	{C("SA"), 174}, {C("SY"), 175}, {C("BH"), 176}, {C("JO"), 177}, {C("SM"), 184}, {C("VA"), 185}, {C("BM"), 186},
};

void CFGService::getCountryCodeID(u32 messagePointer) {
	// Read the character code as a u16 instead of as ASCII, and use it to index the unordered_map above and get the result
	const u16 characterCode = mem.read16(messagePointer + 4);
	log("CFG::GetCountryCodeID (code = %04X)\n", characterCode);

	mem.write32(messagePointer, IPC::responseHeader(0x0A, 2, 0));
	
	// If the character code is valid, return its table ID and a success code
	if (auto search = countryCodeToTableIDMap.find(characterCode); search != countryCodeToTableIDMap.end()) {
		mem.write32(messagePointer + 4, Result::Success);
		mem.write16(messagePointer + 8, search->second);
	}
	
	else {
		Helpers::warn("CFG::GetCountryCodeID: Invalid country code %X", characterCode);
		mem.write32(messagePointer + 4, Result::CFG::NotFound);
		mem.write16(messagePointer + 8, 0xFF);
	}
}

void CFGService::getCountryCodeString(u32 messagePointer) {
	const u16 id = mem.read16(messagePointer + 4);
	log("CFG::getCountryCodeString (id = %04X)\n", id);

	mem.write32(messagePointer, IPC::responseHeader(0x09, 2, 0));

	for (auto [string, code] : countryCodeToTableIDMap) {
		if (code == id) {
			mem.write32(messagePointer + 4, Result::Success);
			mem.write32(messagePointer + 8, u32(string));
			return;
		}
	}

	// Code is not a valid country code, return an appropriate error
	mem.write32(messagePointer + 4, 0xD90103FA);
}

void CFGService::secureInfoGetByte101(u32 messagePointer) {
	log("CFG::SecureInfoGetByte101\n");

	mem.write32(messagePointer, IPC::responseHeader(0x407, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, 0); // Secure info byte 0x101 is usually 0 according to 3DBrew
}

void CFGService::getLocalFriendCodeSeed(u32 messagePointer) {
	log("CFG::GetLocalFriendCodeSeed\n");

	mem.write32(messagePointer, IPC::responseHeader(0x405, 3, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write64(messagePointer + 8, 0);
}

void CFGService::setConfigInfoBlk4(u32 messagePointer) {
	u32 blockID = mem.read32(messagePointer + 4);
	u32 size = mem.read32(messagePointer + 8);
	u32 input = mem.read32(messagePointer + 16);
	log("CFG::SetConfigInfoBlk4 (block ID = %X, size = %X, input pointer = %08X)\n", blockID, size, input);

	mem.write32(messagePointer, IPC::responseHeader(0x401, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, IPC::pointerHeader(0, size, IPC::BufferType::Receive));
	mem.write32(messagePointer + 12, input);
}

void CFGService::updateConfigNANDSavegame(u32 messagePointer) {
	log("CFG::UpdateConfigNANDSavegame\n");

	mem.write32(messagePointer, IPC::responseHeader(0x403, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

// https://www.3dbrew.org/wiki/Cfg:TranslateCountryInfo
void CFGService::translateCountryInfo(u32 messagePointer) {
	const u32 country = mem.read32(messagePointer + 4);
	const u8 direction = mem.read8(messagePointer + 8);
	log("CFG::TranslateCountryInfo (country = %d, direction = %d)\n", country, direction);

	// By default the translated code is  the input
	u32 result = country;

	if (direction == 0) { // Translate from version B to version A
		switch (country) {
			case 0x6E040000: result = 0x6E030000; break;
			case 0x6E050000: result = 0x6E040000; break;
			case 0x6E060000: result = 0x6E050000; break;
			case 0x6E070000: result = 0x6E060000; break;
			case 0x6E030000: result = 0x6E070000; break;
			default: break;
		}
	} else if (direction == 1) { // Translate from version A to version B
		switch (country) {
			case 0x6E030000: result = 0x6E040000; break;
			case 0x6E040000: result = 0x6E050000; break;
			case 0x6E050000: result = 0x6E060000; break;
			case 0x6E060000: result = 0x6E070000; break;
			case 0x6E070000: result = 0x6E030000; break;
			default: break;
		}
	}

	mem.write32(messagePointer, IPC::responseHeader(0x8, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, result);
}

void CFGService::isFangateSupported(u32 messagePointer) {
	log("CFG::IsFangateSupported\n");

	// TODO: What even is fangate?
	mem.write32(messagePointer, IPC::responseHeader(0xB, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 1);
}

void CFGService::norInitialize(u32 messagePointer) {
	log("CFG::NOR::Initialize\n");

	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void CFGService::norReadData(u32 messagePointer) {
	log("CFG::NOR::ReadData\n");
	Helpers::warn("Unimplemented CFG::NOR::ReadData");
	
	mem.write32(messagePointer, IPC::responseHeader(0x5, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}