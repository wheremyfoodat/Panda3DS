#include <algorithm>
#include <memory>

#include "fs/archive_card_spi.hpp"

namespace fs = std::filesystem;

HorizonResult CardSPIArchive::createFile(const FSPath& path, u64 size) {
	Helpers::panic("[Card SPI] CreateFile not yet supported");
	return Result::Success;
}

HorizonResult CardSPIArchive::deleteFile(const FSPath& path) {
	Helpers::panic("[Card SPI] Unimplemented DeleteFile");
	return Result::Success;
}

HorizonResult CardSPIArchive::createDirectory(const FSPath& path) {
	Helpers::panic("[Card SPI] CreateDirectory not yet supported");
	return Result::Success;
}

FileDescriptor CardSPIArchive::openFile(const FSPath& path, const FilePerms& perms) {
	Helpers::panic("[Card SPI] OpenFile not yet supported");
	return FileError;
}

Rust::Result<ArchiveBase*, HorizonResult> CardSPIArchive::openArchive(const FSPath& path) {
	if (!path.isEmptyType()) {
		Helpers::panic("Unimplemented path type for CardSPIArchive::OpenArchive");
	}

	Helpers::warn("Unimplemented: Card SPI archive");
	return Err(Result::FailurePlaceholder);
}

Rust::Result<DirectorySession, HorizonResult> CardSPIArchive::openDirectory(const FSPath& path) {
	Helpers::panic("[Card SPI] OpenDirectory not yet supported");
	return Err(Result::FailurePlaceholder);
}
