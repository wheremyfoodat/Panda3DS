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

	exeFS.offset = info.offset + u64(*(u32*)&header[0x1A0]) * mediaUnit;
	exeFS.size = u64(*(u32*)&header[0x1A4]) * mediaUnit;
	exeFS.hashRegionSize = u64(*(u32*)&header[0x1A8]) * mediaUnit;

	romFS.offset = info.offset + u64(*(u32*)&header[0x1B0]) * mediaUnit;
	romFS.size = u64(*(u32*)&header[0x1B4]) * mediaUnit;
	romFS.hashRegionSize = u64(*(u32*)&header[0x1B8]) * mediaUnit;

	if (encrypted) {
		Crypto::AESKey primaryKeyY;
		Crypto::AESKey secondaryKeyY;
		std::memcpy(primaryKeyY.data(), header, primaryKeyY.size());

		if (!seedCrypto) {
			secondaryKeyY = primaryKeyY;
		} else {
			Helpers::panic("Seed crypto is not supported");
			return false;
		}

		auto primaryResult = getPrimaryKey(aesEngine, primaryKeyY);

		if (!primaryResult.first) {
			Helpers::panic("getPrimaryKey failed!");
			return false;
		}

		Crypto::AESKey primaryKey = primaryResult.second;

		auto secondaryResult = getSecondaryKey(aesEngine, secondaryKeyY);

		if (!secondaryResult.first) {
			Helpers::panic("getSecondaryKey failed!");
			return false;
		}

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
			printf("NCSD is supposedly ecrypted but not actually encrypted\n");
			encrypted = false;
		}
		// If it's truly encrypted, we need to read section again.
		if (encrypted) {
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
		u64 exeFSOffset = fileOffset + exeFS.offset; // Offset of ExeFS in the file = exeFS offset + ncch offset
		printf("ExeFS offset: %08llX, size: %08llX (Offset in file = %08llX)\n", exeFS.offset, exeFS.size, exeFSOffset);
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
			}
		}
	}

	if (hasRomFS()) {
		printf("RomFS offset: %08llX, size: %08llX\n", romFS.offset, romFS.size);
	}

	if (stackSize != 0 && stackSize != VirtualAddrs::DefaultStackSize) {
		Helpers::warn("Requested stack size is %08X bytes. Temporarily emulated as 0x4000 until adjustable sizes are added\n", stackSize);
	}

	initialized = true;
	return true;
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