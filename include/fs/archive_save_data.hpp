#pragma once
#include "archive_base.hpp"

class SaveDataArchive : public ArchiveBase {

public:
	SaveDataArchive(Memory& mem) : ArchiveBase(mem) {}

	u64 getFreeBytes() override { return 0; }
	const char* name() override { return "SaveData"; }

	bool openFile(const FSPath& path) override;
	ArchiveBase* openArchive(const FSPath& path) override;
	std::optional<u32> readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) override;
};