#pragma once
#include "archive_base.hpp"

class SDMCArchive : public ArchiveBase {

public:
	SDMCArchive(Memory& mem) : ArchiveBase(mem) {}

	u64 getFreeBytes() override { return 0; }
	const char* name() override { return "SDMC"; }

	bool openFile(const FSPath& path) override;
	ArchiveBase* openArchive(const FSPath& path) override;
	std::optional<u32> readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) override;
};