#pragma once
#include "archive_base.hpp"
#include "result/result.hpp"

using Result::HorizonResult;

class SDMCArchive : public ArchiveBase {
	bool isWriteOnly = false;  // There's 2 variants of the SDMC archive: Regular one (Read/Write) and write-only

  public:
	SDMCArchive(Memory& mem, bool writeOnly = false) : ArchiveBase(mem), isWriteOnly(writeOnly) {}

	u64 getFreeBytes() override { return 1_GB; }
	std::string name() override { return "SDMC"; }

	HorizonResult createFile(const FSPath& path, u64 size) override;
	HorizonResult deleteFile(const FSPath& path) override;
	HorizonResult createDirectory(const FSPath& path) override;

	std::expected<ArchiveBase*, HorizonResult> openArchive(const FSPath& path) override;
	std::expected<DirectorySession, HorizonResult> openDirectory(const FSPath& path) override;

	FileDescriptor openFile(const FSPath& path, const FilePerms& perms) override;
	std::optional<u32> readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) override;
};