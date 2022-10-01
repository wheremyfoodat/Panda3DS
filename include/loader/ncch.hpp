#pragma once
#include "helpers.hpp"

struct NCCH {
    bool isNew3DS = false;
    bool initialized = false;
    bool compressExeFS = false;
    bool mountRomFS = false;
    bool encrypted = false;

    static constexpr u64 mediaUnit = 0x200;
    u64 size = 0; // Size of NCCH converted to bytes
    u32 stackSize = 0;
    u32 bssSize = 0;

    // Header: 0x200 + 0x800 byte NCCH header + exheadr
    // Returns true on success, false on failure
    bool loadFromHeader(u8* header);
};