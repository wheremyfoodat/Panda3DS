#pragma once
#include "helpers.hpp"
#include <string>
#include <vector>
#include <memory>

namespace RomFS {

    struct RomFSNode {
        std::u16string name {};
        // The file/directory offset relative to the start of the RomFS
        u64 offset { 0 };
        u64 size { 0 };
        bool isDirectory { false };

        std::vector<std::unique_ptr<RomFSNode>> directories {};
        std::vector<std::unique_ptr<RomFSNode>> files {};
    };

    std::unique_ptr<RomFSNode> parseRomFSTree(uintptr_t romFS, u64 romFSSize);
    
} // namespace RomFS