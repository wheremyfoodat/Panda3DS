#pragma once
#include "archive_base.hpp"

class ExtSaveDataArchive : public ArchiveBase {
public:
	ExtSaveDataArchive(Memory& mem, const std::string& folder, bool isShared = false) : ArchiveBase(mem),
		isShared(isShared), backingFolder(folder) {}

	u64 getFreeBytes() override { Helpers::panic("ExtSaveData::GetFreeBytes unimplemented"); return 0;  }
	std::string name() override { return "ExtSaveData::" + backingFolder; }

	HorizonResult createDirectory(const FSPath& path) override;
	HorizonResult deleteDirectory(const FSPath& path) override;
	HorizonResult deleteDirectoryRecursively(const FSPath& path) override;
	HorizonResult createFile(const FSPath& path, u64 size) override;
	HorizonResult deleteFile(const FSPath& path) override;
	HorizonResult renameFile(const FSPath& oldPath, const FSPath& newPath) override;

	Rust::Result<ArchiveBase*, HorizonResult> openArchive(const FSPath& path) override;
	Rust::Result<DirectorySession, HorizonResult> openDirectory(const FSPath& path) override;
	FileDescriptor openFile(const FSPath& path, const FilePerms& perms) override;
	std::optional<u32> readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) override;
	void format(const FSPath& path, const FormatInfo& info) override;
	Rust::Result<FormatInfo, HorizonResult> getFormatInfo(const FSPath& path) override;
	void clear(const FSPath& path) const;

	std::filesystem::path getFormatInfoPath(const FSPath& path) const;
	std::filesystem::path getUserDataPath() const;

	// Takes in a binary ExtSaveData path, outputs a combination of the backing folder with the low and high save entries of the path
	// Used for identifying the archive format info files
	std::string getExtSaveDataPathFromBinary(const FSPath& path) const;

	bool isShared = false;
	std::string backingFolder; // Backing folder for the archive. Can be NAND, Gamecard or SD depending on the archive path.
};
