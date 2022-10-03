#include <vector>
#include "loader/lz77.hpp"
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

        file.seek(exeFSOffset);
        auto [success, bytes] = file.readBytes(exeFSHeader, exeFSHeaderSize);
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
                    file.seek(exeFSOffset + exeFSHeaderSize + fileOffset); 
                    file.readBytes(tmp.data(), fileSize);
                    
                    // Decompress .code file from the tmp vector to the "code" vector
                    if (!CartLZ77::decompress(codeFile, tmp)) {
                        printf("Failed to decompress .code file\n");
                        return false;
                    }
                } else {
                    codeFile.resize(fileSize);
                    file.seek(exeFSOffset + exeFSHeaderSize + fileOffset);
                    file.readBytes(codeFile.data(), fileSize);
                }
            }
        }
    }

    if (hasRomFS()) {
        printf("RomFS offset: %08llX, size: %08llX\n", romFS.offset, romFS.size);
    }

    if (stackSize != 0 && stackSize != VirtualAddrs::DefaultStackSize) {
        Helpers::panic("Stack size != 0x4000");
    }

    if (encrypted) {
        if (hasExeFS())
            Helpers::panic("Encrypted NCCH partition with ExeFS");
        else
            printf("Encrypted NCCH partition. Hopefully not required because it doesn't have an ExeFS. Skipped\n");
    }

    initialized = true;
    return true;
}