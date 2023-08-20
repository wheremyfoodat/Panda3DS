#pragma once
#include "archive_base.hpp"
#include "result/result.hpp"

using Result::HorizonResult;

class SDMCArchive : public ArchiveBase {
public:
	SDMCArchive(Memory& mem) : ArchiveBase(mem) {}

	u64 getFreeBytes() override { return 1_GB; }
	std::string name() override { return "SDMC"; }

	HorizonResult createFile(const FSPath& path, u64 size) override;
	HorizonResult deleteFile(const FSPath& path) override;

	Rust::Result<ArchiveBase*, HorizonResult> openArchive(const FSPath& path) override;
	FileDescriptor openFile(const FSPath& path, const FilePerms& perms) override;
	std::optional<u32> readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) override;
};