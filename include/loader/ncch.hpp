#pragma once
#include <array>
#include "helpers.hpp"

struct NCCH {
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

    // Header: 0x200 + 0x800 byte NCCH header + exheadr
    // Returns true on success, false on failure
    bool loadFromHeader(u8* header);
    bool hasExtendedHeader() { return exheaderSize != 0; }

private:
    std::array<u8, 16> primaryKey = {}; // For exheader, ExeFS header and icons
    std::array<u8, 16> secondaryKey = {}; // For RomFS and some files in ExeFS
};