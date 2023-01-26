#pragma once
#include "archive_base.hpp"

class NCCHArchive : public ArchiveBase {
public:
	NCCHArchive(Memory& mem) : ArchiveBase(mem) {}

	u64 getFreeBytes() override { Helpers::panic("NCCH::GetFreeBytes unimplemented"); return 0;  }
	std::string name() override { return "NCCH"; }

	ArchiveBase* openArchive(const FSPath& path) override;
	CreateFileResult createFile(const FSPath& path, u64 size) override;
	FileDescriptor openFile(const FSPath& path, const FilePerms& perms) override;
	std::optional<u32> readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) override;
};