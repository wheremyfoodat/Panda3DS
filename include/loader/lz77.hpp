#pragma once
#include <vector>
#include "helpers.hpp"

// For parsing the LZ77 format used for compressing the .code file in the ExeFS
namespace CartLZ77 {
    // Retrieves the uncompressed size of the compressed LZ77 data stored in buffer with a specific compressed size
    u32 decompressedSize(const u8* buffer, u32 compressedSize);

    template <typename T>
    static u32 decompressedSize(const std::vector<T>& buffer) {
        return decompressedSize((u8*)buffer.data(), u32(buffer.size() * sizeof(T)));
    }

    // Decompresses an LZ77-compressed buffer stored in input to output
    bool decompress(std::vector<u8>& output, const std::vector<u8>& input);
} // End namespace CartLZ77
