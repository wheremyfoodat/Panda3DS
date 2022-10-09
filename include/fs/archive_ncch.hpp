#include "archive_base.hpp"

class SelfNCCHArchive : public ArchiveBase {

public:
	SelfNCCHArchive(Memory& mem) : ArchiveBase(mem) {}
	
	u64 getFreeBytes() override { return 0; }
	const char* name() override { return "SelfNCCH"; }

	bool openFile(const FSPath& path) override {
		if (path.type != PathType::Binary) {
			printf("Invalid SelfNCCH path type");
			return false;
		}

		return true;
	}

	ArchiveBase* openArchive(FSPath& path) override {
		if (path.type != PathType::Empty) {
			printf("Invalid path type for SelfNCCH archive: %d\n", path.type);
			return nullptr;
		}

		return this;
	}
};