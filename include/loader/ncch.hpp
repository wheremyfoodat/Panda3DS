#pragma once
#include <array>
#include <optional>
#include <vector>

#include "crypto/aes_engine.hpp"
#include "helpers.hpp"
#include "io_file.hpp"
#include "services/region_codes.hpp"

struct NCCH {
	struct EncryptionInfo {
		Crypto::AESKey normalKey;
		Crypto::AESKey initialCounter;
	};

	struct FSInfo {  // Info on the ExeFS/RomFS
		u64 offset = 0;
		u64 size = 0;
		u64 hashRegionSize = 0;
		std::optional<EncryptionInfo> encryptionInfo;
	};

	// Descriptions for .text, .data and .rodata sections
	struct CodeSetInfo {
		u32 address = 0;
		u32 pageCount = 0;
		u32 size = 0;

		// Extract the code set info from the relevant header data
		void extract(const u8 *headerEntry) {
			address = *(u32 *)&headerEntry[0];
			pageCount = *(u32 *)&headerEntry[4];
			size = *(u32 *)&headerEntry[8];
		}
	};

	u64 partitionIndex = 0;
	u64 fileOffset = 0;

	bool isNew3DS = false;
	bool initialized = false;
	bool compressCode = false;  // Shows whether the .code file in the ExeFS is compressed
	bool mountRomFS = false;
	bool encrypted = false;
	bool fixedCryptoKey = false;
	bool seedCrypto = false;
	u8 secondaryKeySlot = 0;

	static constexpr u64 mediaUnit = 0x200;
	u64 size = 0;  // Size of NCCH converted to bytes
	u32 stackSize = 0;
	u32 bssSize = 0;
	u32 exheaderSize = 0;

	FSInfo exheaderInfo;
	FSInfo exeFS;
	FSInfo romFS;
	CodeSetInfo text, data, rodata;
	FSInfo partitionInfo;

	// Contents of the .code file in the ExeFS
	std::vector<u8> codeFile;
	// Contains of the cart's save data
	std::vector<u8> saveData;
	// The cart region. Only the CXI's region matters to us. Necessary to get past region locking
	std::optional<Regions> region = std::nullopt;
	std::vector<u8> smdh;

	// Returns true on success, false on failure
	// Partition index/offset/size must have been set before this
	bool loadFromHeader(Crypto::AESEngine &aesEngine, IOFile &file, const FSInfo &info);

	bool hasExtendedHeader() { return exheaderSize != 0; }
	bool hasExeFS() { return exeFS.size != 0; }
	bool hasRomFS() { return romFS.size != 0; }
	bool hasCode() { return codeFile.size() != 0; }
	bool hasSaveData() { return saveData.size() != 0; }

	// Parse SMDH for region info and such. Returns false on failure, true on success
	bool parseSMDH(const std::vector<u8> &smdh);

	std::pair<bool, Crypto::AESKey> getPrimaryKey(Crypto::AESEngine &aesEngine, const Crypto::AESKey &keyY);
	std::pair<bool, Crypto::AESKey> getSecondaryKey(Crypto::AESEngine &aesEngine, const Crypto::AESKey &keyY);

	std::pair<bool, std::size_t> readFromFile(IOFile &file, const FSInfo &info, u8 *dst, std::size_t offset, std::size_t size);
};