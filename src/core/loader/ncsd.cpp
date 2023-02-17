#include <cstring>
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
        NCCH& ncch = partition.ncch;
        partition.offset = u64(partitionData[i * 2]) * NCSD::mediaUnit;
        partition.length = u64(partitionData[i * 2 + 1]) * NCSD::mediaUnit;

        ncch.partitionIndex = i;
        ncch.fileOffset = partition.offset;

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

            if (!ncch.loadFromHeader(ncchHeader, ncsd.file)) {
                printf("Invalid NCCH partition\n");
                return std::nullopt;
            }
        }
    }
    
    auto& cxi = ncsd.partitions[0].ncch;
    if (!cxi.hasExtendedHeader() || !cxi.hasCode()) {
        printf("NCSD with an invalid CXI in partition 0?\n");
        return std::nullopt;
    }

    printf("Text address = %08X, size = %08X\n", cxi.text.address, cxi.text.size);
    printf("Rodata address = %08X, size = %08X\n", cxi.rodata.address, cxi.rodata.size);
    printf("Data address = %08X, size = %08X\n", cxi.data.address, cxi.data.size);
    
    // Map code file to memory
    auto& code = cxi.codeFile;
    u32 bssSize = (cxi.bssSize + 0xfff) & ~0xfff; // Round BSS size up to a page boundary
    // Total memory to allocate for loading
    u32 totalSize = (cxi.text.pageCount + cxi.rodata.pageCount + cxi.data.pageCount) * pageSize + bssSize;
    code.resize(code.size() + bssSize, 0); // Pad the .code file with zeroes for the BSS segment

    if (code.size() < totalSize) {
        Helpers::panic("Total code size as reported by the exheader is larger than the .code file");
        return std::nullopt;
    }

    const auto opt = findPaddr(totalSize);
    if (!opt.has_value()) {
        Helpers::panic("Failed to find paddr to map CXI file's code to");
        return std::nullopt;
    }

    const auto paddr = opt.value();
    std::memcpy(&fcram[paddr], &code[0], totalSize); // Copy the 3 segments + BSS to FCRAM

    // Map the ROM on the kernel side
    u32 textOffset = 0;
    u32 textAddr = cxi.text.address;
    u32 textSize = cxi.text.pageCount * pageSize;

    u32 rodataOffset = textOffset + textSize;
    u32 rodataAddr = cxi.rodata.address;
    u32 rodataSize = cxi.rodata.pageCount * pageSize;

    u32 dataOffset = rodataOffset + rodataSize;
    u32 dataAddr = cxi.data.address;
    u32 dataSize = cxi.data.pageCount * pageSize + bssSize; // We're merging the data and BSS segments, as BSS is just pre-initted .data

    allocateMemory(textAddr, paddr + textOffset, textSize, true, true, false, true); // Text is R-X
    allocateMemory(rodataAddr, paddr + rodataOffset, rodataSize, true, true, false, false); // Rodata is R--
    allocateMemory(dataAddr, paddr + dataOffset, dataSize, true, true, true, false); // Data+BSS is RW-

    ncsd.entrypoint = textAddr;

    // Back the IOFile for accessing the ROM, as well as the ROM's CXI partition, in the memory class.
    CXIFile = ncsd.file;
    loadedCXI = cxi;

    return ncsd;
}