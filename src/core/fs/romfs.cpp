#include "fs/romfs.hpp"
#include "fs/ivfc.hpp"
#include <cstdio>

namespace RomFS {

    RomFSNode parseRomFSTree(uintptr_t romFS, u64 romFSSize) {
        RomFSNode root;

        IVFC::IVFC ivfc;
        size_t ivfcSize = IVFC::parseIVFC((uintptr_t)romFS, ivfc);

        if (ivfcSize == 0) {
            printf("Failed to parse IVFC\n");
            return {};
        }

        return root;
    }

} // namespace RomFS