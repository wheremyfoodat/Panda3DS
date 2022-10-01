#include "helpers.hpp"

struct NCCH {
    bool isNew3DS = false;
    bool initialized = false;
    bool mountRomFS = false;
    bool encrypted = false;

    static constexpr u64 mediaUnit = 0x200;
    u64 size = 0; // Size of NCCH converted to bytes

    // Header: 0x200 byte NCCH header
    // Returns true on success, false on failure
    bool loadFromHeader(u8* header);
};