#pragma once
#include <array>
#include "io_file.hpp"
#include "helpers.hpp"

struct NCCH {
    struct FSInfo { // Info on the ExeFS/RomFS
        u64 offset = 0;
        u64 size = 0;
        u64 hashRegionSize = 0;
    };

    u64 partitionIndex = 0;
    u64 fileOffset = 0;

    bool isNew3DS = false;
    bool initialized = false;
    bool compressExeFS = false;
    bool mountRomFS = false;
    bool encrypted = false;
    bool fixedCryptoKey = false;

    static constexpr u64 mediaUnit = 0x200;
    u64 size = 0; // Size of NCCH converted to bytes
    u32 stackSize = 0;
    u32 bssSize = 0;
    u32 exheaderSize = 0;

    FSInfo exeFS;
    FSInfo romFS;

    // Header: 0x200 + 0x800 byte NCCH header + exheadr
    // Returns true on success, false on failure
    // Partition index/offset/size must have been set before this
    bool loadFromHeader(u8* header, IOFile& file);

    bool hasExtendedHeader() { return exheaderSize != 0; }
    bool hasExeFS() { return exeFS.size != 0; }
    bool hasRomFS() { return romFS.size != 0; }

private:
    std::array<u8, 16> primaryKey = {}; // For exheader, ExeFS header and icons
    std::array<u8, 16> secondaryKey = {}; // For RomFS and some files in ExeFS
};