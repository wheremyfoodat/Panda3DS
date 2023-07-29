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

	inline constexpr uintptr_t alignUp(uintptr_t value, uintptr_t alignment) {
		if (value % alignment == 0) return value;

		return value + (alignment - (value % alignment));
	}

	void printNode(const RomFSNode& node, int indentation) {
		for (int i = 0; i < indentation; i++) {
			printf("  ");
		}
		printf("%s/\n", std::string(node.name.begin(), node.name.end()).c_str());

		for (auto& file : node.files) {
			for (int i = 0; i <= indentation; i++) {
				printf("  ");
			}
			printf("%s\n", std::string(file->name.begin(), file->name.end()).c_str());
		}

		indentation++;
		for (auto& directory : node.directories) {
			printNode(*directory, indentation);
		}
		indentation--;
	}

	std::vector<std::unique_ptr<RomFSNode>> getFiles(uintptr_t fileMetadataBase, u32 currentFileOffset) {
		std::vector<std::unique_ptr<RomFSNode>> files;

		while (currentFileOffset != metadataInvalidEntry) {
			u32* metadataPtr = (u32*)(fileMetadataBase + currentFileOffset);
			metadataPtr++;  // Skip the containing directory
			u32 nextFileOffset = *metadataPtr++;
			u64 fileDataOffset = *(u64*)metadataPtr;
			metadataPtr += 2;
			u64 fileSize = *(u64*)metadataPtr;
			metadataPtr += 2;
			metadataPtr++;  // Skip the offset of the next file in the same hash table bucket
			u32 nameLength = *metadataPtr++ / 2;

			// Arbitrary limit
			if (nameLength > 128) {
				printf("Invalid file name length: %08X\n", nameLength);
				return {};
			}

			char16_t* namePtr = (char16_t*)metadataPtr;
			std::u16string name(namePtr, nameLength);

			std::unique_ptr file = std::make_unique<RomFSNode>();
			file->isDirectory = false;
			file->name = name;
			file->metadataOffset = currentFileOffset;
			file->dataOffset = fileDataOffset;
			file->dataSize = fileSize;

			files.push_back(std::move(file));

			currentFileOffset = nextFileOffset;
		}

		return files;
	}

	std::unique_ptr<RomFSNode> parseRootDirectory(uintptr_t directoryMetadataBase, uintptr_t fileMetadataBase) {
		std::unique_ptr<RomFSNode> rootDirectory = std::make_unique<RomFSNode>();
		rootDirectory->isDirectory = true;
		rootDirectory->name = u"romfs:";
		rootDirectory->metadataOffset = 0;

		u32 rootFilesOffset = *((u32*)(directoryMetadataBase) + 3);
		if (rootFilesOffset != metadataInvalidEntry) {
			rootDirectory->files = getFiles(fileMetadataBase, rootFilesOffset);
		}

		std::queue<RomFSNode*> directoryOffsets;
		directoryOffsets.push(rootDirectory.get());

		while (!directoryOffsets.empty()) {
			RomFSNode* currentNode = directoryOffsets.front();
			directoryOffsets.pop();

			u32* metadataPtr = (u32*)(directoryMetadataBase + currentNode->metadataOffset);
			metadataPtr += 2;

			// Offset of first child directory
			u32 currentDirectoryOffset = *metadataPtr;

			// Loop over all the sibling directories of the first child to get all the children directories
			// of the current directory
			while (currentDirectoryOffset != metadataInvalidEntry) {
				metadataPtr = (u32*)(directoryMetadataBase + currentDirectoryOffset);
				metadataPtr++;  // Skip the parent offset
				u32 siblingDirectoryOffset = *metadataPtr++;
				metadataPtr++;  // Skip offset of first child directory
				u32 currentFileOffset = *metadataPtr++;
				metadataPtr++;  // Skip offset of next directory in the same hash table bucket
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
				directory->metadataOffset = currentDirectoryOffset;
				directory->files = getFiles(fileMetadataBase, currentFileOffset);

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

		uintptr_t masterHashOffset = RomFS::alignUp(ivfcSize, 0x10);
		// From GBATEK:
		// The "Logical Offsets" are completely unrelated to the physical offsets in the RomFS partition.
		// Instead, the "Logical Offsets" might be something about where to map the Level 1-3 sections in
		// virtual memory (with the physical Level 3,1,2 ordering being re-ordered to Level 1,2,3)?
		uintptr_t level3Offset = RomFS::alignUp(masterHashOffset + ivfc.masterHashSize, ivfc.levels[2].blockSize);
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

		std::unique_ptr<RomFSNode> root = parseRootDirectory(level3Base + header.directoryMetadataOffset, level3Base + header.fileMetadataOffset);

		// If you want to print the tree, uncomment this
		// printNode(*root, 0);

		return root;
	}

}  // namespace RomFS