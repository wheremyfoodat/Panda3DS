#pragma once
#include "archive_base.hpp"

class ExtSaveDataArchive : public ArchiveBase {
public:
	ExtSaveDataArchive(Memory& mem) : ArchiveBase(mem) {}

	u64 getFreeBytes() override { Helpers::panic("ExtSaveData::GetFreeBytes unimplemented"); return 0;  }
	std::string name() override { return "ExtSaveData"; }

	bool openFile(const FSPath& path) override;
	ArchiveBase* openArchive(const FSPath& path) override;
	std::optional<u32> readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) override;

	bool isShared = false;
};