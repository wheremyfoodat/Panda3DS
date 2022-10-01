#include <optional>
#include "loader/ncsd.hpp"
#include "memory.hpp"

std::optional<NCSD> Memory::loadNCSD(const std::filesystem::path& path) {
    NCSD ncsd;
    if (!ncsd.file.open(path, "rb"))
        return std::nullopt;

    u8 magic[4]; // Must be "NCSD"
    ncsd.file.seek(0x100);
    auto [success, bytes] = ncsd.file.readBytes(magic, 4);

    if (!success || bytes != 4) {
        printf("Failed to read NCSD magic\n");
        return std::nullopt;
    }

    if (magic[0] != 'N' || magic[1] != 'C' || magic[2] != 'S' || magic[3] != 'D') {
        printf("NCSD with wrong magic value\n");
        return std::nullopt;
    }

    std::tie(success, bytes) = ncsd.file.readBytes(&ncsd.size, 4);
    if (!success || bytes != 4) {
        printf("Failed to read NCSD size\n");
        return std::nullopt;
    }

    ncsd.size *= NCSD::mediaUnit; // Convert size to bytes

    // Read partition data
    ncsd.file.seek(0x120);
    // 2 u32s per partition (offset and length), 8 partitions total
    constexpr size_t partitionDataSize = 8 * 2; // Size of partition in u32s
    u32 partitionData[8 * 2];
    std::tie(success, bytes) = ncsd.file.read(partitionData, partitionDataSize, sizeof(u32));
    if (!success || bytes != partitionDataSize) {
        printf("Failed to read NCSD partition data\n");
        return std::nullopt;
    }

    for (int i = 0; i < 8; i++) {
        auto& partition = ncsd.partitions[i];
        partition.offset = u64(partitionData[i * 2]) * NCSD::mediaUnit;
        partition.length = u64(partitionData[i * 2 + 1]) * NCSD::mediaUnit;

        if (partition.length != 0) { // Initialize the NCCH of each partition
            ncsd.file.seek(partition.offset);

            // 0x200 bytes for the NCCH header and another 0x800 for the exheader
            constexpr u64 headerSize = 0x200 + 0x800;
            u8 ncchHeader[headerSize];
            std::tie(success, bytes) = ncsd.file.readBytes(ncchHeader, headerSize);
            if (!success || bytes != headerSize) {
                printf("Failed to read NCCH header\n");
                return std::nullopt;
            }

            if (!partition.ncch.loadFromHeader(ncchHeader)) {
                printf("Invalid NCCH partition\n");
                return std::nullopt;
            }
        }
    }

    return ncsd;
}