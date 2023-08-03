#include <algorithm>
#include <cstring>
#include "loader/lz77.hpp"
#include "bit_helpers.hpp"

// The difference in size between the compressed and decompressed file is stored
// As a footer in the compressed file. To get the decompressed size, we extract the diff
// And add it to the compressed size
u32 CartLZ77::decompressedSize(const u8* buffer, u32 compressedSize) {
    u32 sizeDiff;
    std::memcpy(&sizeDiff, buffer + compressedSize - 4, sizeof(u32));
    return sizeDiff + compressedSize;
}

bool CartLZ77::decompress(std::vector<u8>& output, const std::vector<u8>& input) {
    u32 sizeCompressed = u32(input.size() * sizeof(u8));
    u32 sizeDecompressed = decompressedSize(input);
    output.resize(sizeDecompressed);

    const u8* compressed = (u8*)input.data();
    const u8* footer = compressed + sizeCompressed - 8;

    u32 bufferTopAndBottom;
    std::memcpy(&bufferTopAndBottom, footer, sizeof(u32));

    u32 out = sizeDecompressed; // TODO: Is this meant to be u32 or s32?
    u32 index = sizeCompressed - (Helpers::getBits<24, 8>(bufferTopAndBottom));
    u32 stopIndex = sizeCompressed - (bufferTopAndBottom & 0xffffff);

    // Set all of the decompressed buffer to 0 and copy the compressed buffer to the start of it
    std::fill(output.begin(), output.end(), 0);
    std::copy(input.begin(), input.end(), output.begin());

    while (index > stopIndex) {
        u8 control = compressed[--index];

        for (uint i = 0; i < 8; i++) {
            if (index <= stopIndex)
                break;
            if (index <= 0)
                break;
            if (out <= 0)
                break;

            if (control & 0x80) {
                // Check if compression is out of bounds
                if (index < 2)
                    return false;
                index -= 2;

                u32 segmentOffset = compressed[index] | (compressed[index + 1] << 8);
                u32 segment_size = (Helpers::getBits<12, 4>(segmentOffset)) + 3;
                segmentOffset &= 0x0FFF;
                segmentOffset += 2;

                // Check if compression is out of bounds
                if (out < segment_size)
                    return false;

                for (uint j = 0; j < segment_size; j++) {
                    // Check if compression is out of bounds
                    if (out + segmentOffset >= sizeDecompressed)
                        return false;

                    u8 data = output[out + segmentOffset];
                    output[--out] = data;
                }
            }
            else {
                // Check if compression is out of bounds
                if (out < 1)
                    return false;
                output[--out] = compressed[--index];
            }
            control <<= 1;
        }
    }

    return true;
}
