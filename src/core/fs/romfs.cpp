#include "fs/romfs.hpp"

#include <cstdio>
#include <queue>
#include <string>

#include "fs/ivfc.hpp"
#include "helpers.hpp"

namespace RomFS {

	constexpr u32 metadataInvalidEntry = 0xFFFFFFFF;

	struct Level3Header {
		u32 headerSize;
		u32 directoryHashTableOffset;
		u32 directoryHashTableSize;
		u32 directoryMetadataOffset;
		u32 directoryMetadataSize;
		u32 fileHashTableOffset;
		u32 fileHashTableSize;
		u32 fileMetadataOffset;
		u32 fileMetadataSize;
		u32 fileDataOffset;
	};

	inline uintptr_t align(uintptr_t value, uintptr_t alignment) {
		if (value % alignment == 0) return value;

		return value + (alignment - (value % alignment));
	}

	inline void printNode(const RomFSNode& node, int indentation, std::string path) {
		for (int i = 0; i < indentation; i++) {
			printf("  ");
		}
		printf("%s%s\n", path.c_str(), std::string(node.name.begin(), node.name.end()).c_str());
		path += std::string(node.name.begin(), node.name.end()) + "/";
		indentation++;
		for (auto& directory : node.directories) {
			printNode(*directory, indentation, path);
		}
		indentation--;
	}

	std::unique_ptr<RomFSNode> parseRootDirectory(uintptr_t metadataBase) {
		std::unique_ptr<RomFSNode> rootDirectory = std::make_unique<RomFSNode>();
		rootDirectory->isDirectory = true;
		rootDirectory->name = u"romfs:";
		rootDirectory->offset = 0;

		std::queue<RomFSNode*> directoryOffsets;
		directoryOffsets.push(rootDirectory.get());

		while (!directoryOffsets.empty()) {
			RomFSNode* currentNode = directoryOffsets.front();
			directoryOffsets.pop();

			u32* metadataPtr = (u32*)(metadataBase + currentNode->offset);
			metadataPtr += 2;

			// Offset of first child directory
			u32 currentDirectoryOffset = *metadataPtr;

			// Loop over all the sibling directories of the first child to get all the children directories
			// of the current directory
			while (currentDirectoryOffset != metadataInvalidEntry) {
				metadataPtr = (u32*)(metadataBase + currentDirectoryOffset);
				metadataPtr++;  // Skip the parent offset
				u32 siblingDirectoryOffset = *metadataPtr;
				// Skip the rest of the fields
				metadataPtr += 4;
				u32 nameLength = *metadataPtr++ / 2;

				// Arbitrary limit
				if (nameLength > 128) {
					printf("Invalid directory name length: %08X\n", nameLength);
					return {};
				}

				char16_t* namePtr = (char16_t*)metadataPtr;
				std::u16string name(namePtr, nameLength);

				std::unique_ptr directory = std::make_unique<RomFSNode>();
				directory->isDirectory = true;
				directory->name = name;
				directory->offset = currentDirectoryOffset;
				currentNode->directories.push_back(std::move(directory));

				currentDirectoryOffset = siblingDirectoryOffset;
			}

			for (auto& directory : currentNode->directories) {
				directoryOffsets.push(directory.get());
			}
		}

		return rootDirectory;
	}

	std::unique_ptr<RomFSNode> parseRomFSTree(uintptr_t romFS, u64 romFSSize) {
		IVFC::IVFC ivfc;
		size_t ivfcSize = IVFC::parseIVFC((uintptr_t)romFS, ivfc);

		if (ivfcSize == 0) {
			printf("Failed to parse IVFC\n");
			return {};
		}

		uintptr_t masterHashOffset = RomFS::align(ivfcSize, 0x10);
		// For a weird reason, the level 3 offset is not the one in the IVFC, instead it's
		// the first block after the master hash
		// TODO: Find out why and explain in the comment
		uintptr_t level3Offset = RomFS::align(masterHashOffset + ivfc.masterHashSize, ivfc.levels[2].blockSize);
		uintptr_t level3Base = (uintptr_t)romFS + level3Offset;
		u32* level3Ptr = (u32*)level3Base;

		Level3Header header;
		header.headerSize = *level3Ptr++;
		header.directoryHashTableOffset = *level3Ptr++;
		header.directoryHashTableSize = *level3Ptr++;
		header.directoryMetadataOffset = *level3Ptr++;
		header.directoryMetadataSize = *level3Ptr++;
		header.fileHashTableOffset = *level3Ptr++;
		header.fileHashTableSize = *level3Ptr++;
		header.fileMetadataOffset = *level3Ptr++;
		header.fileMetadataSize = *level3Ptr++;
		header.fileDataOffset = *level3Ptr;

		if (header.headerSize != 0x28) {
			printf("Invalid level 3 header size: %08X\n", header.headerSize);
			return {};
		}

		std::unique_ptr<RomFSNode> root = parseRootDirectory(level3Base + header.directoryMetadataOffset);

		// If you want to print the tree, uncomment this
		// printNode(*root, 0, "");

		return root;
	}

}  // namespace RomFS