#pragma once
#include <array>
#include <memory>
#include <optional>
#include "common_types.h"

namespace Teakra {
struct SharedMemory {
    // We allocate our own memory if the user doesn't supply their own
    std::unique_ptr<std::array<u8, 0x80000>> own_memory;
    // Points to either own own memory or user-supplied memory
    u8* raw;

    SharedMemory(u8* mem = nullptr) : raw{mem} {
        if (mem == nullptr) {
            own_memory = std::make_unique<std::array<u8, 0x80000>>();
            raw = own_memory->data();
        }
    }

    u16 ReadWord(u32 word_address) const {
        u32 byte_address = word_address * 2;
        u8 low = raw[byte_address];
        u8 high = raw[byte_address + 1];
        return low | ((u16)high << 8);
    }
    void WriteWord(u32 word_address, u16 value) {
        u8 low = value & 0xFF;
        u8 high = value >> 8;
        u32 byte_address = word_address * 2;
        raw[byte_address] = low;
        raw[byte_address + 1] = high;
    }
};
} // namespace Teakra
