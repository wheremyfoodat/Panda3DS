#pragma once
#include <optional>
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

    static const char* toString(u32 id) {
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
    u32 type;
    u32 size;
    u32 pointer; // Pointer to the actual path data
};

class ArchiveBase;

struct FileSession {
    ArchiveBase* archive = nullptr;
    FSPath path;
    bool isOpen;

    FileSession(ArchiveBase* archive, const FSPath& filePath, bool isOpen = true) : archive(archive), isOpen(isOpen) {
        path = filePath;
    }
};

struct ArchiveSession {
    ArchiveBase* archive = nullptr;
    FSPath path;
    bool isOpen;

    ArchiveSession(ArchiveBase* archive, const FSPath& filePath, bool isOpen = true) : archive(archive), isOpen(isOpen) {
        path = filePath;
    }
};

class ArchiveBase {
protected:
    using Handle = u32;
    Memory& mem;

public:
    virtual const char* name() = 0;
    virtual u64 getFreeBytes() = 0;
    virtual bool openFile(const FSPath& path) = 0;

    virtual ArchiveBase* openArchive(const FSPath& path) = 0;

    // Read size bytes from a file starting at offset "offset" into a certain buffer in memory
    // Returns the number of bytes read, or nullopt if the read failed
    virtual std::optional<u32> readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) = 0;
    
    ArchiveBase(Memory& mem) : mem(mem) {}
};