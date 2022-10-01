#include <array>
#include "helpers.hpp"
#include "io_file.hpp"
#include "loader/ncch.hpp"

struct NCSD {
    static constexpr u64 mediaUnit = 0x200;

    struct Partition {
        u64 offset = 0; // Offset of partition converted to bytes
        u64 length = 0; // Length of partition converted to bytes
        NCCH ncch;
    };

    IOFile file;
    u64 size = 0; // Image size according to the header converted to bytes
    std::array<Partition, 8> partitions; // NCCH partitions
};
