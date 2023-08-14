#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <cstring>
#include <vector>
#include "loader/lz77.hpp"
#include "loader/ncch.hpp"
#include "memory.hpp"

#include <iostream>

bool NCCH::loadFromHeader(Crypto::AESEngine &aesEngine, IOFile& file, const FSInfo &info) {
    // 0x200 bytes for the NCCH header
    constexpr u64 headerSize = 0x200;
    u8 header[headerSize];

    auto [success, bytes] = readFromFile(file, info, header, 0, headerSize);
    if (!success || bytes != headerSize) {
        printf("Failed to read NCCH header\n");
        return false;
    }
	
	if (header[0x100] != 'N' || header[0x101] != 'C' || header[0x102] != 'C' || header[0x103] != 'H') {
		printf("Invalid header on NCCH\n");
		return false;
	}

	codeFile.clear();
	saveData.clear();

	size = u64(*(u32*)&header[0x104]) * mediaUnit; // TODO: Maybe don't type pun because big endian will break
	exheaderSize = *(u32*)&header[0x180];

	const u64 programID = *(u64*)&header[0x118];

	// Read NCCH flags
	secondaryKeySlot = header[0x188 + 3];
	isNew3DS = header[0x188 + 4] == 2;
	fixedCryptoKey = (header[0x188 + 7] & 0x1) == 0x1;
	mountRomFS = (header[0x188 + 7] & 0x2) != 0x2;
	encrypted = (header[0x188 + 7] & 0x4) != 0x4;
	seedCrypto = (header[0x188 + 7] & 0x20) == 0x20;

	// Read exheader, ExeFS and RomFS info
	exheaderInfo.offset = info.offset + 0x200;
	exheaderInfo.size = exheaderSize;
	exheaderInfo.hashRegionSize = 0;
	exheaderInfo.encryptionInfo = std::nullopt;

	exeFS.offset = info.offset + u64(*(u32*)&header[0x1A0]) * mediaUnit;
	exeFS.size = u64(*(u32*)&header[0x1A4]) * mediaUnit;
	exeFS.hashRegionSize = u64(*(u32*)&header[0x1A8]) * mediaUnit;
	exeFS.encryptionInfo = std::nullopt;

	romFS.offset = info.offset + u64(*(u32*)&header[0x1B0]) * mediaUnit;
	romFS.size = u64(*(u32*)&header[0x1B4]) * mediaUnit;
	romFS.hashRegionSize = u64(*(u32*)&header[0x1B8]) * mediaUnit;
	romFS.encryptionInfo = std::nullopt;

	// Shows whether we got the primary and secondary keys correctly
	bool gotCryptoKeys = true;
	if (encrypted) {
		Crypto::AESKey primaryKeyY;
		Crypto::AESKey secondaryKeyY;
		std::memcpy(primaryKeyY.data(), header, primaryKeyY.size());

		if (!seedCrypto) {
			secondaryKeyY = primaryKeyY;
		} else {
			Helpers::warn("Seed crypto is not supported");
			gotCryptoKeys = false;
		}

		auto primaryResult = getPrimaryKey(aesEngine, primaryKeyY);
		auto secondaryResult = getSecondaryKey(aesEngine, secondaryKeyY);

		if (!primaryResult.first || !secondaryResult.first) {
			gotCryptoKeys = false;
			if (!primaryResult.first) printf("Do not have primary key\n");
			if (!secondaryResult.first) printf("Do not have secondary key\n");
		} else {
			Crypto::AESKey primaryKey = primaryResult.second;
			Crypto::AESKey secondaryKey = secondaryResult.second;

			EncryptionInfo encryptionInfoTmp;
			encryptionInfoTmp.normalKey = primaryKey;
			encryptionInfoTmp.initialCounter.fill(0);

			for (std::size_t i = 1; i <= sizeof(std::uint64_t) - 1; i++) {
				encryptionInfoTmp.initialCounter[i] = header[0x108 + sizeof(std::uint64_t) - 1 - i];
			}
			encryptionInfoTmp.initialCounter[8] = 1;
			exheaderInfo.encryptionInfo = encryptionInfoTmp;

			encryptionInfoTmp.initialCounter[8] = 2;
			exeFS.encryptionInfo = encryptionInfoTmp;

			encryptionInfoTmp.normalKey = secondaryKey;
			encryptionInfoTmp.initialCounter[8] = 3;
			romFS.encryptionInfo = encryptionInfoTmp;
		}
	}

	if (exheaderSize != 0) {
		std::unique_ptr<u8[]> exheader(new u8[exheaderSize]);

		auto [success, bytes] = readFromFile(file, info, &exheader[0], 0x200, exheaderSize);
		if (!success || bytes != exheaderSize) {
			printf("Failed to read Extended NCCH header\n");
			return false;
		}

		const u64 jumpID = *(u64*)&exheader[0x1C0 + 0x8];

		// It seems like some decryption tools will decrypt the file, without actually setting the NoCrypto flag in the NCCH header
		// This is a nice and easy hack to see if a file is pretending to be encrypted, taken from 3DMoo and Citra
		if (u32(programID) == u32(jumpID) && encrypted) {
			printf("NCSD is supposedly encrypted but not actually encrypted\n");
			encrypted = false;

			// Cartridge is not actually encrypted, set all of our encryption info structures to nullopt
			exheaderInfo.encryptionInfo = std::nullopt;
			romFS.encryptionInfo = std::nullopt;
			exeFS.encryptionInfo = std::nullopt;
		}

		// If it's truly encrypted, we need to read section again.
		if (encrypted) {
			if (!aesEngine.haveKeys()) {
				Helpers::panic(
					"Loaded an encrypted ROM but AES keys don't seem to have been provided correctly! Navigate to the emulator's\n"
					"app data folder and make sure you have a sysdata directory with a file called aes_keys.txt which contains your keys!"
				);
				return false;
			}

			if (!gotCryptoKeys) {
				Helpers::panic("ROM is encrypted but it seems we couldn't get either the primary or the secondary key");
				return false;
			}

			auto [success, bytes] = readFromFile(file, exheaderInfo, &exheader[0], 0, exheaderSize);
			if (!success || bytes != exheaderSize) {
				printf("Failed to read Extended NCCH header\n");
				return false;
			}
		}

		const u64 saveDataSize = *(u64*)&exheader[0x1C0 + 0x0]; // Size of save data in bytes
		saveData.resize(saveDataSize, 0xff);

		compressCode = (exheader[0xD] & 1) != 0;
		stackSize = *(u32*)&exheader[0x1C];
		bssSize = *(u32*)&exheader[0x3C];

		text.extract(&exheader[0x10]);
		rodata.extract(&exheader[0x20]);
		data.extract(&exheader[0x30]);
	}

	printf("Stack size: %08X\nBSS size: %08X\n", stackSize, bssSize);

	// Read ExeFS
	if (hasExeFS()) {
		// Offset of ExeFS in the file = exeFS offset + NCCH offset
		// exeFS.offset has already been offset by the NCCH offset
		printf("ExeFS offset: %08llX, size: %08llX (Offset in file = %08llX)\n", exeFS.offset - info.offset, exeFS.size, exeFS.offset);
		constexpr size_t exeFSHeaderSize = 0x200;

		u8 exeFSHeader[exeFSHeaderSize];

		auto [success, bytes] = readFromFile(file, exeFS, exeFSHeader, 0, exeFSHeaderSize);
		if (!success || bytes != exeFSHeaderSize) {
			printf("Failed to parse ExeFS header\n");
			return false;
		}

		// ExeFS format allows up to 10 files
		for (int i = 0; i < 10; i++) {
			u8* fileInfo = &exeFSHeader[i * 16];

			char name[9];
			std::memcpy(name, fileInfo, 8); // Get file name as a string
			name[8] = '\0'; // Add null terminator to it just in case there's none

			u32 fileOffset = *(u32*)&fileInfo[0x8];
			u32 fileSize = *(u32*)&fileInfo[0xC];

			if (fileSize != 0) {
				printf("File %d. Name: %s, Size: %08X, Offset: %08X\n", i, name, fileSize, fileOffset);
			}

			if (std::strcmp(name, ".code") == 0) {
				if (hasCode()) {
					Helpers::panic("Second code file in a single NCCH partition. What should this do?\n");
				}

				if (compressCode) {
					std::vector<u8> tmp;
					tmp.resize(fileSize);

					// A file offset of 0 means our file is located right after the ExeFS header
					// So in the ROM, files are located at (file offset + exeFS offset + exeFS header size)
					readFromFile(file, exeFS, tmp.data(), fileOffset + exeFSHeaderSize, fileSize);
					
					// Decompress .code file from the tmp vector to the "code" vector
					if (!CartLZ77::decompress(codeFile, tmp)) {
						printf("Failed to decompress .code file\n");
						return false;
					}
				} else {
					codeFile.resize(fileSize);
					readFromFile(file, exeFS, codeFile.data(), fileOffset + exeFSHeaderSize, fileSize);
				}
			} else if (std::strcmp(name, "icon") == 0) {
				// Parse icon file to extract region info and more in the future (logo, etc)
				std::vector<u8> tmp;
				tmp.resize(fileSize);
				readFromFile(file, exeFS, tmp.data(), fileOffset + exeFSHeaderSize, fileSize);

				if (!parseSMDH(tmp)) {
					printf("Failed to parse SMDH!\n");
				}
			}
		}
	}

	// If no region has been detected for CXI, set the region to USA by default
	if (!region.has_value() && partitionIndex == 0) {
		printf("No region detected for CXI, defaulting to USA\n");
		region = Regions::USA;
	}

	if (hasRomFS()) {
		printf("RomFS offset: %08llX, size: %08llX\n", romFS.offset, romFS.size);
	}

	initialized = true;
	return true;
}

bool NCCH::parseSMDH(const std::vector<u8>& smdh) {
	if (smdh.size() < 0x36C0) {
		printf("The cartridge .icon file is too small, considered invalid. Must be 0x36C0 bytes minimum\n");
		return false;
	}

	if (char(smdh[0]) != 'S' || char(smdh[1]) != 'M' || char(smdh[2]) != 'D' || char(smdh[3]) != 'H') {
		printf("Invalid SMDH magic!\n");
		return false;
	}

	// Bitmask showing which regions are allowed.
	// https://www.3dbrew.org/wiki/SMDH#Region_Lockout
	const u32 regionMasks = *(u32*)&smdh[0x2018];
	// Detect when games are region free (ie all regions are allowed) for future use
	[[maybe_unused]] const bool isRegionFree = (regionMasks & 0x7f) == 0x7f;

	// See which countries are allowed
	const bool japan = (regionMasks & 0x1) != 0;
	const bool northAmerica = (regionMasks & 0x2) != 0;
	const bool europe = (regionMasks & 0x4) != 0;
	const bool australia = (regionMasks & 0x8) != 0;
	const bool china = (regionMasks & 0x10) != 0;
	const bool korea = (regionMasks & 0x20) != 0;
	const bool taiwan = (regionMasks & 0x40) != 0;

	// Based on the allowed regions, set the autodetected 3DS region. We currently prefer English-speaking regions for practical purposes.
	// But this should be configurable later.
	if (northAmerica) {
		region = Regions::USA;
	} else if (europe) {
		region = Regions::Europe;
	} else if (australia) {
		region = Regions::Australia;
	} else if (japan) {
		region = Regions::Japan;
	} else if (korea) {
		region = Regions::Korea;
	} else if (china) {
		region = Regions::China;
	} else if (taiwan) {
		region = Regions::Taiwan;
	}
}

std::pair<bool, Crypto::AESKey> NCCH::getPrimaryKey(Crypto::AESEngine &aesEngine, const Crypto::AESKey &keyY) {
	Crypto::AESKey result;

	if (encrypted) {
		if (fixedCryptoKey) {
			return {true, result};
		}

		aesEngine.setKeyY(Crypto::KeySlotId::NCCHKey0, keyY);

		if (!aesEngine.hasNormalKey(Crypto::KeySlotId::NCCHKey0)) {
			return {false, result};
		}

		result = aesEngine.getNormalKey(Crypto::KeySlotId::NCCHKey0);
	}

	return {true, result};
}

std::pair<bool, Crypto::AESKey> NCCH::getSecondaryKey(Crypto::AESEngine &aesEngine, const Crypto::AESKey &keyY) {
	Crypto::AESKey result;

	if (encrypted) {

		if (fixedCryptoKey) {
			return {true, result};
		}

		Crypto::KeySlotId keySlotId;

		switch (secondaryKeySlot) {
			case 0:
				keySlotId = Crypto::KeySlotId::NCCHKey0;
				break;
			case 1:
				keySlotId = Crypto::KeySlotId::NCCHKey1;
				break;
			case 10:
				keySlotId = Crypto::KeySlotId::NCCHKey2;
				break;
			case 11:
				keySlotId = Crypto::KeySlotId::NCCHKey3;
				break;
			default:
				return {false, result};
		}

		if (!aesEngine.hasKeyX(keySlotId)) {
			return {false, result};
		}

		aesEngine.setKeyY(keySlotId, keyY);

		if (!aesEngine.hasNormalKey(keySlotId)) {
			return {false, result};
		}

		result = aesEngine.getNormalKey(keySlotId);
	}

	return {true, result};
}

std::pair<bool, std::size_t> NCCH::readFromFile(IOFile& file, const FSInfo &info, u8 *dst, std::size_t offset, std::size_t size) {
	if (size == 0) {
		return { true, 0 };
	}

	std::size_t readMaxSize = std::min(size, static_cast<std::size_t>(info.size) - offset);

	file.seek(info.offset + offset);
	auto [success, bytes] = file.readBytes(dst, readMaxSize);

	if (!success) {
		return { success, bytes};
	}

	if (success && info.encryptionInfo.has_value()) {
		auto& encryptionInfo = info.encryptionInfo.value();

		CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption d(encryptionInfo.normalKey.data(), encryptionInfo.normalKey.size(), encryptionInfo.initialCounter.data());

		if (offset > 0) {
			d.Seek(offset);
		}

		CryptoPP::byte* data = reinterpret_cast<CryptoPP::byte*>(dst);
		d.ProcessData(data, data, bytes);
	}

	return { success, bytes};
}
