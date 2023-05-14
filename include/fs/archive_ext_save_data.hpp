#pragma once
#include "archive_base.hpp"

class ExtSaveDataArchive : public ArchiveBase {
public:
	ExtSaveDataArchive(Memory& mem, const std::string& folder, bool isShared = false) : ArchiveBase(mem),
		isShared(isShared), backingFolder(folder) {}

	u64 getFreeBytes() override { Helpers::panic("ExtSaveData::GetFreeBytes unimplemented"); return 0;  }
	std::string name() override { return "ExtSaveData::" + backingFolder; }

	FSResult createFile(const FSPath& path, u64 size) override;
	FSResult deleteFile(const FSPath& path) override;

	Rust::Result<ArchiveBase*, FSResult> openArchive(const FSPath& path) override;
	Rust::Result<DirectorySession, FSResult> openDirectory(const FSPath& path) override;
	FileDescriptor openFile(const FSPath& path, const FilePerms& perms) override;
	std::optional<u32> readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) override;

	bool isShared = false;
	std::string backingFolder; // Backing folder for the archive. Can be NAND, Gamecard or SD depending on the archive path.
};