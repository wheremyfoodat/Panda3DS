#pragma once
#include <array>
#include "helpers.hpp"
#include "io_file.hpp"
#include "loader/ncch.hpp"

struct HB3DSX {
    // File layout:
    // - File header
    // - Code, rodata and data relocation table headers
    // - Code segment
    // - Rodata segment
    // - Loadable (non-BSS) part of the data segment
    // - Code relocation table
    // - Rodata relocation table
    // - Data relocation table

    // Memory layout before relocations are applied:
    // [0..codeSegSize)             -> code segment
    // [codeSegSize..rodataSegSize) -> rodata segment
    // [rodataSegSize..dataSegSize) -> data segment

    // Memory layout after relocations are applied: well, however the loader sets it up :)
    // The entrypoint is always the start of the code segment.
    // The BSS section must be cleared manually by the application.

    // File header
    struct Header {
        // minus char magic[4]
        u16 headerSize;
        u16 relocHeaderSize;
        u32 formatVer;
        u32 flags;

        // Sizes of the code, rodata and data segments +
        // size of the BSS section (uninitialized latter half of the data segment)
        u32 codeSegSize, rodataSegSize, dataSegSize, bssSize;
    };

    // Relocation header: all fields (even extra unknown fields) are guaranteed to be relocation counts.
    struct RelocHeader {
        u32 absoluteCount; // # of absolute relocations (that is, fix address to post-relocation memory layout)
        u32 relativeCount; // # of cross-segment relative relocations (that is, 32bit signed offsets that need to be patched)
        // more?

        // Relocations are written in this order:
        // - Absolute relocs
        // - Relative relocs
    };

    enum class RelocType {
        Absolute,
        Relative,
    };

    // Relocation entry: from the current pointer, skip X words and patch Y words
    struct Reloc {
        u16 skip, patch;
    };

    // _prm structure
    static constexpr std::array<char, 4> PRM_MAGIC = {'_', 'P', 'R', 'M'};
    struct PrmStruct {
        char magic[4];
        u32 pSrvOverride;
        u32 aptAppId;
        u32 heapSize, linearHeapSize;
        u32 pArgList;
        u32 runFlags;
    };

    IOFile file;

    static constexpr u32 entrypoint = 0x00100000; // Initial ARM11 PC
    u32 romFSSize = 0;
    u32 romFSOffset = 0;

    bool hasRomFs() const;
    std::pair<bool, std::size_t> readRomFSBytes(void *dst, std::size_t offset, std::size_t size);
};
