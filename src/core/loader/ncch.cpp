#include <cryptopp/aes.h>
#include "loader/ncch.hpp"
#include "memory.hpp"

bool NCCH::loadFromHeader(u8* header, IOFile& file) {
    if (header[0x100] != 'N' || header[0x101] != 'C' || header[0x102] != 'C' || header[0x103] != 'H') {
        printf("Invalid header on NCCH\n");
        return false;
    }

    size = u64(*(u32*)&header[0x104]) * mediaUnit; // TODO: Maybe don't type pun because big endian will break
    exheaderSize = *(u32*)&header[0x180];

    const u64 programID = *(u64*)&header[0x118];

    // Read NCCH flags
    isNew3DS = header[0x188 + 4] == 2;
    fixedCryptoKey = (header[0x188 + 7] & 0x1) == 0x1;
    mountRomFS = (header[0x188 + 7] & 0x2) != 0x2;
    encrypted = (header[0x188 + 7] & 0x4) != 0x4;
    
    // Read ExeFS and RomFS info
    exeFS.offset = u64(*(u32*)&header[0x1A0]) * mediaUnit;
    exeFS.size = u64(*(u32*)&header[0x1A4]) * mediaUnit;
    exeFS.hashRegionSize = u64(*(u32*)&header[0x1A8]) * mediaUnit;

    romFS.offset = u64(*(u32*)&header[0x1B0]) * mediaUnit;
    romFS.size = u64(*(u32*)&header[0x1B4]) * mediaUnit;
    romFS.hashRegionSize = u64(*(u32*)&header[0x1B8]) * mediaUnit;

    if (fixedCryptoKey) {
        Helpers::panic("Fixed crypto keys for NCCH");
    }

    if (exheaderSize != 0) {
        const u8* exheader = &header[0x200]; // Extended NCCH header
        const u64 jumpID = *(u64*)&exheader[0x1C0 + 0x8];

        // It seems like some decryption tools will decrypt the file, without actually setting the NoCrypto flag in the NCCH header
        // This is a nice and easy hack to see if a file is pretending to be encrypted, taken from 3DMoo and Citra
        if (u32(programID) == u32(jumpID) && encrypted) {
            printf("NCSD is supposedly ecrypted but not actually encrypted\n");
            encrypted = false;
        } else if (encrypted) {
            Helpers::panic("Encrypted NCSD file");
        }

        compressExeFS = (exheader[0xD] & 1) != 0;
        stackSize = *(u32*)&exheader[0x1C];
        bssSize = *(u32*)&exheader[0x3C];
    }

    printf("Stack size: %08X\nBSS size: %08X\n", stackSize, bssSize);

    // Read ExeFS
    if (hasExeFS()) {
        printf("ExeFS offset: %08llX, size: %08llX\n", exeFS.offset, exeFS.size);
        auto exeFSOffset = fileOffset + exeFS.offset;
        constexpr size_t exeFSHeaderSize = 0x200;

        u8 exeFSHeader[exeFSHeaderSize];

        file.seek(exeFSOffset);
        auto [success, bytes] = file.readBytes(exeFSHeader, exeFSHeaderSize);
        if (!success || bytes != exeFSHeaderSize) {
            printf("Failed to parse ExeFS header\n");
            return false;
        }

        // ExeFS format allows up to 10 files
        for (int file = 0; file < 10; file++) {
            u8* fileInfo = &exeFSHeader[file * 16];

            char name[9];
            std::memcpy(name, fileInfo, 8); // Get file name as a string
            name[8] = '\0'; // Add null terminator to it just in case there's none

            u32 fileOffset = *(u32*)&fileInfo[0x8];
            u32 fileSize = *(u32*)&fileInfo[0xC];

            if (fileSize != 0) {
                printf("File %d. Name: %s, Size: %08X, Offset: %08X\n", file, name, fileSize, fileOffset);
            }
        }
    }

    if (hasRomFS()) {
        printf("RomFS offset: %08llX, size: %08llX\n", romFS.offset, romFS.size);
    }

    if (stackSize != VirtualAddrs::DefaultStackSize) {
        Helpers::panic("Stack size != 0x4000");
    }

    if (compressExeFS) {
        Helpers::panic("Compressed ExeFS");
    }

    if (encrypted) {
        Helpers::panic("Encrypted NCCH partition");
    }

    initialized = true;
    return true;
}