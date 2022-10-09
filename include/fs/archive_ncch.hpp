#include "archive_base.hpp"

class SelfNCCHArchive : public ArchiveBase {

public:
	SelfNCCHArchive(Memory& mem) : ArchiveBase(mem) {}

	u64 getFreeBytes() override { return 0; }
	const char* name() override { return "SelfNCCH"; }

	bool openFile(const FSPath& path) override;
	ArchiveBase* openArchive(FSPath& path) override;
	std::optional<u32> readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) override;
};