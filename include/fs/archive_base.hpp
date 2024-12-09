#pragma once
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "helpers.hpp"
#include "memory.hpp"
#include "result.hpp"
#include "result/result.hpp"

using Result::HorizonResult;

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
		SDMCWriteOnly = 0xA,

		CardSPI = 0x12345679,
		SavedataAndNcch = 0x2345678A,
		// 3DBrew: This is the same as the regular SaveData archive, except with this the savedata ID and mediatype is loaded from the input archive
		// lowpath.
		UserSaveData1 = 0x567890B2,
		// 3DBrew: Similar to 0x567890B2 but can only access Accessible Save specified in exheader?
		UserSaveData2 = 0x567890B4,

		TwlPhoto = 0x567890AC,
		TwlSound = 0x567890AD,
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
			case SavedataAndNcch: return "Savedata & NCCH (archive 0x2345678A)";
			case TwlPhoto: return "TWL_PHOTO";
			case TwlSound: return "TWL_SOUND";
			default: return "Unknown archive";
		}
	}
}  // namespace ArchiveID

struct FSPath {
	u32 type = PathType::Invalid;

	std::vector<u8> binary;  // Path data for binary paths
	std::string string;      // Path data for ASCII paths
	std::u16string utf16_string;

	FSPath() {}

	FSPath(u32 type, const std::vector<u8>& vec) : type(type) {
		switch (type) {
			case PathType::Binary: binary = std::move(vec); break;

			case PathType::ASCII:
				string.resize(vec.size() - 1);                           // -1 because of the null terminator
				std::memcpy(string.data(), vec.data(), vec.size() - 1);  // Copy string data
				break;

			case PathType::UTF16: {
				const size_t size = vec.size() / sizeof(u16) - 1;  // Character count. -1 because null terminator here too
				utf16_string.resize(size);
				std::memcpy(utf16_string.data(), vec.data(), size * sizeof(u16));
				break;
			};
		}
	}
};

struct FilePerms {
	u32 raw;

	FilePerms(u32 val) : raw(val) {}
	bool read() const { return (raw & 1) != 0; }
	bool write() const { return (raw & 2) != 0; }
	bool create() const { return (raw & 4) != 0; }
};

class ArchiveBase;
struct FileSession {
	ArchiveBase* archive = nullptr;
	FILE* fd = nullptr;  // File descriptor for file sessions that require them.
	FSPath path;
	FSPath archivePath;
	u32 priority = 0;  // TODO: What does this even do
	bool isOpen;

	FileSession(ArchiveBase* archive, const FSPath& filePath, const FSPath& archivePath, FILE* fd, bool isOpen = true)
		: archive(archive), path(filePath), archivePath(archivePath), fd(fd), isOpen(isOpen), priority(0) {}

	// For cloning a file session
	FileSession(const FileSession& other)
		: archive(other.archive), path(other.path), archivePath(other.archivePath), fd(other.fd), isOpen(other.isOpen), priority(other.priority) {}
};

struct ArchiveSession {
	ArchiveBase* archive = nullptr;
	FSPath path;
	bool isOpen;

	ArchiveSession(ArchiveBase* archive, const FSPath& filePath, bool isOpen = true) : archive(archive), path(filePath), isOpen(isOpen) {}
};

struct DirectoryEntry {
	std::filesystem::path path;
	bool isDirectory;
};

struct DirectorySession {
	ArchiveBase* archive = nullptr;
	// For directories which are mirrored to a specific path on the disk, this contains that path
	// Otherwise this is a nullopt
	std::optional<std::filesystem::path> pathOnDisk;

	// The list of directory entries + the index of the entry we're currently inspecting
	std::vector<DirectoryEntry> entries;
	size_t currentEntry;

	bool isOpen;

	DirectorySession(ArchiveBase* archive, std::filesystem::path path, bool isOpen = true) : archive(archive), pathOnDisk(path), isOpen(isOpen) {
		currentEntry = 0;  // Start from entry 0

		// Read all directory entries, cache them
		for (auto& e : std::filesystem::directory_iterator(path)) {
			DirectoryEntry entry;
			entry.path = e.path();
			entry.isDirectory = std::filesystem::is_directory(e);
			entries.push_back(entry);
		}
	}
};

// Represents a file descriptor obtained from OpenFile. If the optional is nullopt, opening the file failed.
// Otherwise the fd of the opened file is returned (or nullptr if the opened file doesn't require one)
using FileDescriptor = std::optional<FILE*>;

class ArchiveBase {
  public:
	struct FormatInfo {
		u32 size;              // Archive size
		u32 numOfDirectories;  // Number of directories
		u32 numOfFiles;        // Number of files
		bool duplicateData;    // Whether to duplicate data or not
	};

  protected:
	using Handle = u32;

	static constexpr FileDescriptor NoFile = nullptr;
	static constexpr FileDescriptor FileError = std::nullopt;
	Memory& mem;

	// Returns if a specified 3DS path in UTF16 or ASCII format is safe or not
	// A 3DS path is considered safe if its first character is '/' which means we're not trying to access anything outside the root of the fs
	// And if it doesn't contain enough instances of ".." (Indicating "climb up a folder" in filesystems) to let the software climb up the directory
	// tree And access files outside of the emulator's app data folder
	template <u32 format>
	bool isPathSafe(const FSPath& path) {
		static_assert(format == PathType::ASCII || format == PathType::UTF16);
		using String = typename std::conditional<format == PathType::UTF16, std::u16string, std::string>::type;  // String type for the path
		using Char = typename String::value_type;                                                                // Char type for the path

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
	virtual HorizonResult createFile(const FSPath& path, u64 size) = 0;
	virtual HorizonResult deleteFile(const FSPath& path) = 0;

	virtual Rust::Result<FormatInfo, HorizonResult> getFormatInfo(const FSPath& path) {
		Helpers::panic("Unimplemented GetFormatInfo for %s archive", name().c_str());
		// Return a dummy struct just to avoid the UB of not returning anything, even if we panic
		return Ok(FormatInfo{.size = 0, .numOfDirectories = 0, .numOfFiles = 0, .duplicateData = false});
	}

	virtual HorizonResult createDirectory(const FSPath& path) {
		Helpers::panic("Unimplemented CreateDirectory for %s archive", name().c_str());
		return Result::FS::AlreadyExists;
	}

	// Returns nullopt if opening the file failed, otherwise returns a file descriptor to it (nullptr if none is needed)
	virtual FileDescriptor openFile(const FSPath& path, const FilePerms& perms) = 0;
	virtual Rust::Result<ArchiveBase*, HorizonResult> openArchive(const FSPath& path) = 0;

	virtual Rust::Result<DirectorySession, HorizonResult> openDirectory(const FSPath& path) {
		Helpers::panic("Unimplemented OpenDirectory for %s archive", name().c_str());
		return Err(Result::FS::FileNotFoundAlt);
	}

	virtual void format(const FSPath& path, const FormatInfo& info) { Helpers::panic("Unimplemented Format for %s archive", name().c_str()); }

	virtual HorizonResult renameFile(const FSPath& oldPath, const FSPath& newPath) {
		Helpers::panic("Unimplemented RenameFile for %s archive", name().c_str());
		return Result::Success;
	}

	// Read size bytes from a file starting at offset "offset" into a certain buffer in memory
	// Returns the number of bytes read, or nullopt if the read failed
	virtual std::optional<u32> readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) = 0;

	ArchiveBase(Memory& mem) : mem(mem) {}
};

struct ArchiveResource {
	u32 sectorSize;   // Size of a sector in bytes
	u32 clusterSize;  // Size of a cluster in bytes
	u32 partitionCapacityInClusters;
	u32 freeSpaceInClusters;
};