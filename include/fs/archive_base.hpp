#pragma once
#include <cassert>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>
#include "helpers.hpp"
#include "memory.hpp"

namespace PathType {
    enum : u32 {
        Invalid = 0,
        Empty = 1,
        Binary = 2,
        ASCII = 3,
        UTF16 = 4,
    };
}

namespace ArchiveID {
    enum : u32 {
        SelfNCCH = 3,
        SaveData = 4,
        ExtSaveData = 6,
        SharedExtSaveData = 7,
        SystemSaveData = 8,
        SDMC = 9,
        SDMCWriteOnly = 0xA
    };

    static std::string toString(u32 id) {
        switch (id) {
            case SelfNCCH: return "SelfNCCH";
            case SaveData: return "SaveData";
            case ExtSaveData: return "ExtSaveData";
            case SharedExtSaveData: return "SharedExtSaveData";
            case SystemSaveData: return "SystemSaveData";
            case SDMC: return "SDMC";
            case SDMCWriteOnly: return "SDMC (Write-only)";
            default: return "Unknown archive";
        }
    }
}

struct FSPath {
    u32 type = PathType::Invalid;

    std::vector<u8> binary; // Path data for binary paths
    std::string string; // Path data for ASCII paths
    std::u16string utf16_string;

    FSPath() {}

    FSPath(u32 type, std::vector<u8> vec) : type(type) {
        switch (type) {
            case PathType::Binary:
                binary = std::move(vec);
                break;

            case PathType::ASCII:
                string.resize(vec.size() - 1); // -1 because of the null terminator
                std::memcpy(string.data(), vec.data(), vec.size() - 1); // Copy string data
                break;

            case PathType::UTF16: {
                const size_t size = vec.size() / sizeof(u16) - 1; // Character count. -1 because null terminator here too
                utf16_string.resize(size);
                std::memcpy(utf16_string.data(), vec.data(), size * sizeof(u16));
                break;
            }
;        }
    }
};

class ArchiveBase;

struct FileSession {
    ArchiveBase* archive = nullptr;
    FSPath path;
    bool isOpen;

    FileSession(ArchiveBase* archive, const FSPath& filePath, bool isOpen = true) : archive(archive), path(path), isOpen(isOpen) {}
};

struct ArchiveSession {
    ArchiveBase* archive = nullptr;
    FSPath path;
    bool isOpen;

    ArchiveSession(ArchiveBase* archive, const FSPath& filePath, bool isOpen = true) : archive(archive), path(path), isOpen(isOpen) {}
};

class ArchiveBase {
protected:
    using Handle = u32;
    Memory& mem;

    // Returns if a specified 3DS path in UTF16 or ASCII format is safe or not
    // A 3DS path is considered safe if its first character is '/' which means we're not trying to access anything outside the root of the fs
    // And if it doesn't contain enough instances of ".." (Indicating "climb up a folder" in filesystems) to let the software climb up the directory tree
    // And access files outside of the emulator's app data folder
    template <u32 format>
    bool isPathSafe(const FSPath& path) {
        static_assert(format == PathType::ASCII || format == PathType::UTF16);
        using String = std::conditional<format == PathType::UTF16, std::u16string, std::string>::type; // String type for the path
        using Char = String::value_type; // Char type for the path

        String pathString, dots;
        if constexpr (std::is_same<String, std::u16string>()) {
            pathString = path.utf16_string;
            dots = u"..";
        } else {
            pathString = path.string;
            dots = "..";
        }

        // If the path string doesn't begin with / then that means it's accessing outside the FS root, which is invalid & unsafe
        if (pathString[0] != Char('/')) return false;

        // Counts how many folders sans the root our file is nested under. 
        // If it's < 0 at any point of parsing, then the path is unsafe and tries to crawl outside our file sandbox.
        // If it's 0 then this is the FS root.
        // If it's > 0 then we're in a subdirectory of the root.
        int level = 0;
        
        // Split the string on / characters and see how many of the substrings are ".."
        size_t pos = 0;
        while ((pos = pathString.find(Char('/'))) != String::npos) {
            String token = pathString.substr(0, pos);
            pathString.erase(0, pos + 1);

            if (token == dots) {
                level--;
                if (level < 0) return false;
            } else {
                level++;
            }
        }

        return true;
    }

public:
    virtual std::string name() = 0;
    virtual u64 getFreeBytes() = 0;
    virtual bool openFile(const FSPath& path) = 0;

    virtual ArchiveBase* openArchive(const FSPath& path) = 0;

    // Read size bytes from a file starting at offset "offset" into a certain buffer in memory
    // Returns the number of bytes read, or nullopt if the read failed
    virtual std::optional<u32> readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) = 0;
    
    ArchiveBase(Memory& mem) : mem(mem) {}
};