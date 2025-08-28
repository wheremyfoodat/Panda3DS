#pragma once
#include "archive_base.hpp"

class SelfNCCHArchive : public ArchiveBase {
  public:
	SelfNCCHArchive(Memory& mem) : ArchiveBase(mem) {}

	u64 getFreeBytes() override { return 0; }
	std::string name() override { return "SelfNCCH"; }

	HorizonResult createFile(const FSPath& path, u64 size) override;
	HorizonResult deleteFile(const FSPath& path) override;

	Rust::Result<ArchiveBase*, HorizonResult> openArchive(const FSPath& path) override;
	FileDescriptor openFile(const FSPath& path, const FilePerms& perms) override;
	std::optional<u32> readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) override;

	// Returns whether the cart has a RomFS
	bool hasRomFS() {
		auto cxi = mem.getCXI();
		auto hb3dsx = mem.get3DSX();
		return (cxi != nullptr && cxi->hasRomFS()) || (hb3dsx != nullptr && hb3dsx->hasRomFs());
	}

	// Returns whether the cart has an ExeFS (All executable carts should have an ExeFS. This is just here to be safe)
	bool hasExeFS() {
		auto cxi = mem.getCXI();
		return (cxi != nullptr && cxi->hasExeFS());
	}
};