#pragma once
#include "archive_base.hpp"
#include "result/result.hpp"

using Result::HorizonResult;

class TWLPhotoArchive : public ArchiveBase {
  public:
	TWLPhotoArchive(Memory& mem) : ArchiveBase(mem) {}
	std::string name() override { return "TWL_PHOTO"; }

	u64 getFreeBytes() override {
		Helpers::warn("Unimplemented GetFreeBytes for TWLPhoto archive");
		return 32_MB;
	}

	HorizonResult createDirectory(const FSPath& path) override;
	HorizonResult createFile(const FSPath& path, u64 size) override;
	HorizonResult deleteFile(const FSPath& path) override;

	Rust::Result<ArchiveBase*, HorizonResult> openArchive(const FSPath& path) override;
	Rust::Result<DirectorySession, HorizonResult> openDirectory(const FSPath& path) override;

	FileDescriptor openFile(const FSPath& path, const FilePerms& perms) override;

	std::optional<u32> readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) override {
		Helpers::panic("Unimplemented ReadFile for TWL_PHOTO archive");
		return {};
	};
};