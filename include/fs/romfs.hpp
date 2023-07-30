#pragma once
#include <memory>
#include <string>
#include <vector>

#include "helpers.hpp"

namespace RomFS {
	struct RomFSNode {
		std::u16string name;
		// The file/directory offset relative to the start of the RomFS
		u64 metadataOffset = 0;
		u64 dataOffset = 0;
		u64 dataSize = 0;
		bool isDirectory = false;

		std::vector<std::unique_ptr<RomFSNode>> directories;
		std::vector<std::unique_ptr<RomFSNode>> files;
	};

	std::unique_ptr<RomFSNode> parseRomFSTree(uintptr_t romFS, u64 romFSSize);
}  // namespace RomFS