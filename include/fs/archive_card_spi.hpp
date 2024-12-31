#pragma once
#include "archive_base.hpp"
#include "result/result.hpp"

using Result::HorizonResult;

class CardSPIArchive : public ArchiveBase {
  public:
	CardSPIArchive(Memory& mem) : ArchiveBase(mem) {}
	std::string name() override { return "Card SPI"; }

	u64 getFreeBytes() override {
		Helpers::warn("Unimplemented GetFreeBytes for Card SPI archive");
		return 0_MB;
	}

	HorizonResult createDirectory(const FSPath& path) override;
	HorizonResult createFile(const FSPath& path, u64 size) override;
	HorizonResult deleteFile(const FSPath& path) override;

	Rust::Result<ArchiveBase*, HorizonResult> openArchive(const FSPath& path) override;
	Rust::Result<DirectorySession, HorizonResult> openDirectory(const FSPath& path) override;

	FileDescriptor openFile(const FSPath& path, const FilePerms& perms) override;

	std::optional<u32> readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) override {
		Helpers::panic("Unimplemented ReadFile for Card SPI archive");
		return {};
	};
};