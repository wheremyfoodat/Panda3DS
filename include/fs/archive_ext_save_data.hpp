#pragma once
#include "archive_base.hpp"

class ExtSaveDataArchive : public ArchiveBase {
public:
	ExtSaveDataArchive(Memory& mem) : ArchiveBase(mem) {}

	u64 getFreeBytes() override { Helpers::panic("ExtSaveData::GetFreeBytes unimplemented"); return 0;  }
	std::string name() override { return "ExtSaveData"; }

	FSResult createFile(const FSPath& path, u64 size) override;
	FSResult deleteFile(const FSPath& path) override;

	ArchiveBase* openArchive(const FSPath& path) override;
	FileDescriptor openFile(const FSPath& path, const FilePerms& perms) override;
	std::optional<u32> readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) override;

	bool isShared = false;
};