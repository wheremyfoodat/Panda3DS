#include <cryptopp/aes.h>
#include "loader/ncch.hpp"
#include "memory.hpp"

bool NCCH::loadFromHeader(u8* header) {
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

    bool encryptedExheader = encrypted;

    if (fixedCryptoKey) {
        Helpers::panic("Fixed crypto keys for NCCH");
    }

    if (exheaderSize != 0) {
        const u8* exheader = &header[0x200]; // Extended NCCH header
        const u64 jumpID = *(u64*)&exheader[0x1C0 + 0x8];

        // TODO: How does this even work
        if (u32(programID) == u32(jumpID) && encryptedExheader) {
            printf("Exheader is supposedly ecrypted but not actually encrypted\n");
            encryptedExheader = false;
        } else if (encryptedExheader) {
            Helpers::panic("Encrypted exheader");
        }

        compressExeFS = (exheader[0xD] & 1) != 0;
        stackSize = *(u32*)&exheader[0x1C];
        bssSize = *(u32*)&exheader[0x3C];
    }

    printf("Stack size: %08X\nBSS size: %08X\n", stackSize, bssSize);

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