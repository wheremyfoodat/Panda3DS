#pragma once
#include "archive_base.hpp"

class ExtSaveDataArchive : public ArchiveBase {
  public:
	ExtSaveDataArchive(Memory& mem, const std::string& folder, bool isShared = false) : ArchiveBase(mem), isShared(isShared), backingFolder(folder) {}

	u64 getFreeBytes() override {
		Helpers::panic("ExtSaveData::GetFreeBytes unimplemented");
		return 0;
	}
	std::string name() override { return "ExtSaveData::" + backingFolder; }

	HorizonResult createDirectory(const FSPath& path) override;
	HorizonResult createFile(const FSPath& path, u64 size) override;
	HorizonResult deleteFile(const FSPath& path) override;
	HorizonResult renameFile(const FSPath& oldPath, const FSPath& newPath) override;

	std::expected<ArchiveBase*, HorizonResult> openArchive(const FSPath& path) override;
	std::expected<DirectorySession, HorizonResult> openDirectory(const FSPath& path) override;
	FileDescriptor openFile(const FSPath& path, const FilePerms& perms) override;
	std::optional<u32> readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) override;

	std::expected<FormatInfo, HorizonResult> getFormatInfo(const FSPath& path) override {
		Helpers::warn("Stubbed ExtSaveData::GetFormatInfo");
		return FormatInfo{.size = 1_GB, .numOfDirectories = 255, .numOfFiles = 255, .duplicateData = false};
	}

	// Takes in a binary ExtSaveData path, outputs a combination of the backing folder with the low and high save entries of the path
	// Used for identifying the archive format info files
	std::string getExtSaveDataPathFromBinary(const FSPath& path);

	bool isShared = false;
	std::string backingFolder;  // Backing folder for the archive. Can be NAND, Gamecard or SD depending on the archive path.
};