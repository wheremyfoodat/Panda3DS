#pragma once
#include "archive_base.hpp"

class SaveDataArchive : public ArchiveBase {
  public:
	SaveDataArchive(Memory& mem) : ArchiveBase(mem) {}

	u64 getFreeBytes() override { return 32_MB; }
	std::string name() override { return "SaveData"; }

	HorizonResult createDirectory(const FSPath& path) override;
	HorizonResult createFile(const FSPath& path, u64 size) override;
	HorizonResult deleteFile(const FSPath& path) override;

	Rust::Result<ArchiveBase*, HorizonResult> openArchive(const FSPath& path) override;
	Rust::Result<DirectorySession, HorizonResult> openDirectory(const FSPath& path) override;
	FileDescriptor openFile(const FSPath& path, const FilePerms& perms) override;
	std::optional<u32> readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) override;

	void format(const FSPath& path, const FormatInfo& info) override;
	Rust::Result<FormatInfo, HorizonResult> getFormatInfo(const FSPath& path) override;

	std::filesystem::path getFormatInfoPath() {
		return IOFile::getAppData() / "FormatInfo" / "SaveData.format";
	}

	// Returns whether the cart has save data or not
	bool cartHasSaveData() {
		auto cxi = mem.getCXI();
		return (cxi != nullptr && cxi->hasSaveData());  // We need to have a CXI file with more than 0 bytes of save data
	}
};