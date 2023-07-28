#pragma once
#include "helpers.hpp"
#include <vector>

namespace RomFS {

    struct RomFSNode {
        std::vector<RomFSNode> children;
    };

    RomFSNode parseRomFSTree(uintptr_t romFS, u64 romFSSize);
    
} // namespace RomFS