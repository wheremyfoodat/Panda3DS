#pragma once
#include "archive_base.hpp"

class SaveDataArchive : public ArchiveBase {
public:
	SaveDataArchive(Memory& mem) : ArchiveBase(mem) {}

	u64 getFreeBytes() override { Helpers::panic("SaveData::GetFreeBytes unimplemented"); return 0; }
	std::string name() override { return "SaveData"; }

	FSResult createDirectory(const FSPath& path) override;
	FSResult createFile(const FSPath& path, u64 size) override;
	FSResult deleteFile(const FSPath& path) override;
	FormatInfo getFormatInfo(const FSPath& path) override;

	ArchiveBase* openArchive(const FSPath& path) override;
	Rust::Result<DirectorySession, FSResult> openDirectory(const FSPath& path) override;
	FileDescriptor openFile(const FSPath& path, const FilePerms& perms) override;
	std::optional<u32> readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) override;

	// Returns whether the cart has save data or not
	bool cartHasSaveData() {
		auto cxi = mem.getCXI();
		return (cxi != nullptr && cxi->hasSaveData()); // We need to have a CXI file with more than 0 bytes of save data
	}
};