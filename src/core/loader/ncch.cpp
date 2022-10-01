#include "loader/ncch.hpp"

bool NCCH::loadFromHeader(u8* header) {
    const u8* exheader = &header[0x200]; // Extended NCCH header

    if (header[0x100] != 'N' || header[0x101] != 'C' || header[0x102] != 'C' || header[0x103] != 'H') {
        printf("Invalid header on NCCH\n");
        return false;
    }

    size = u64(*(u32*)&header[0x104]) * mediaUnit; // TODO: Maybe don't type pun because big endian will break

    // Read NCCH flags
    isNew3DS = header[0x188 + 4] == 2;
    mountRomFS = (header[0x188 + 7] & 0x1) != 0x1;
    encrypted = (header[0x188 + 7] & 0x4) != 0x4;

    compressExeFS = (exheader[0xD] & 1) != 0;

    if (compressExeFS) {
        Helpers::panic("Compressed ExeFS");
    }

    if (encrypted) {
        Helpers::panic("Encrypted NCCH partition");
    }

    initialized = true;
    return true;
}