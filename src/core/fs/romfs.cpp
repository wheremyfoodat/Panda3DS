#include "fs/romfs.hpp"
#include "fs/ivfc.hpp"
#include "helpers.hpp"
#include <cstdio>
#include <memory>
#include <map>
#include <string>

namespace RomFS {

    constexpr u32 metadataInvalidEntry = 0xFFFFFFFF;

    struct Level3Header {
        uint32_t headerSize;
        uint32_t directoryHashTableOffset;
        uint32_t directoryHashTableSize;
        uint32_t directoryMetadataOffset;
        uint32_t directoryMetadataSize;
        uint32_t fileHashTableOffset;
        uint32_t fileHashTableSize;
        uint32_t fileMetadataOffset;
        uint32_t fileMetadataSize;
        uint32_t fileDataOffset;
    };

    inline uintptr_t align(uintptr_t value, uintptr_t alignment) {
        if (value % alignment == 0)
            return value;

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

    std::vector<std::unique_ptr<RomFSNode>> parseDirectory(const uintptr_t metadataBase, const uintptr_t metadataOffset) {
        std::vector<std::unique_ptr<RomFSNode>> directories {};

        // Get offset of first child directory
        u32* metadataPtr = (u32*)(metadataBase + metadataOffset);
        metadataPtr += 2;
        u32 currentDirectoryOffset = *metadataPtr;

        // Loop over all the sibling directories of the first child to get all the children directories
        while (currentDirectoryOffset != metadataInvalidEntry) {
            metadataPtr = (u32*)(metadataBase + currentDirectoryOffset);
            metadataPtr++; // Skip the parent offset
            u32 siblingDirectoryOffset = *metadataPtr++;
            metadataPtr++; // Skip the child offset
            metadataPtr++; // Skip the first file offset
            metadataPtr++; // Skip the next directory in hash table offset
            u32 nameLength = (*metadataPtr++) / 2;

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
            directories.push_back(std::move(directory));

            currentDirectoryOffset = siblingDirectoryOffset;
        }

        // Loop over all the children directories to get their children
        for (auto& directory : directories) {
            directory->directories = parseDirectory(metadataBase, directory->offset);
        }

        return directories;
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
        uintptr_t const level3Base = (uintptr_t)romFS + level3Offset;
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

        std::unique_ptr<RomFSNode> root = std::make_unique<RomFSNode>();
        root->isDirectory = true;
        root->name = u"";
        root->offset = 0;
        root->directories = parseDirectory(level3Base + header.directoryMetadataOffset, 0);

        // If you want to print the tree, uncomment this
        // printNode(*root, 0, "");

        return root;
    }

} // namespace RomFS