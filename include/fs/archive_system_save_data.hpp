#pragma once
#include "archive_base.hpp"

class SystemSaveDataArchive : public ArchiveBase {
  public:
	SystemSaveDataArchive(Memory& mem) : ArchiveBase(mem) {}

	u64 getFreeBytes() override {
		Helpers::warn("Unimplemented GetFreeBytes for SystemSaveData archive");
		return 32_MB;
	}

	std::string name() override { return "SystemSaveData"; }

	HorizonResult createDirectory(const FSPath& path) override;
	HorizonResult createFile(const FSPath& path, u64 size) override;
	HorizonResult deleteFile(const FSPath& path) override;

	std::expected<ArchiveBase*, HorizonResult> openArchive(const FSPath& path) override;
	std::expected<DirectorySession, HorizonResult> openDirectory(const FSPath& path) override;

	FileDescriptor openFile(const FSPath& path, const FilePerms& perms) override;

	std::optional<u32> readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) override {
		Helpers::panic("Unimplemented ReadFile for SystemSaveData archive");
		return {};
	};
};