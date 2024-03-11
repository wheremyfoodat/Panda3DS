#pragma once
#include "archive_base.hpp"

class NCCHArchive : public ArchiveBase {
  public:
	NCCHArchive(Memory& mem) : ArchiveBase(mem) {}

	u64 getFreeBytes() override {
		Helpers::panic("NCCH::GetFreeBytes unimplemented");
		return 0;
	}
	std::string name() override { return "NCCH"; }

	HorizonResult createFile(const FSPath& path, u64 size) override;
	HorizonResult deleteFile(const FSPath& path) override;

	std::expected<ArchiveBase*, HorizonResult> openArchive(const FSPath& path) override;
	FileDescriptor openFile(const FSPath& path, const FilePerms& perms) override;
	std::optional<u32> readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) override;

	// Returns whether the cart has a RomFS
	bool hasRomFS() {
		auto cxi = mem.getCXI();
		return (cxi != nullptr && cxi->hasRomFS());
	}

	// Returns whether the cart has an ExeFS (All executable carts should have an ExeFS. This is just here to be safe)
	bool hasExeFS() {
		auto cxi = mem.getCXI();
		return (cxi != nullptr && cxi->hasExeFS());
	}
};