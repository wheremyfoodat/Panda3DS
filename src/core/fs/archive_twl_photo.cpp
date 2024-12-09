#include <algorithm>
#include <memory>

#include "fs/archive_twl_photo.hpp"

namespace fs = std::filesystem;

HorizonResult TWLPhotoArchive::createFile(const FSPath& path, u64 size) {
	Helpers::panic("[TWL_PHOTO] CreateFile not yet supported");
	return Result::Success;
}

HorizonResult TWLPhotoArchive::deleteFile(const FSPath& path) {
	Helpers::panic("[TWL_PHOTO] Unimplemented DeleteFile");
	return Result::Success;
}

HorizonResult TWLPhotoArchive::createDirectory(const FSPath& path) {
	Helpers::panic("[TWL_PHOTO] CreateDirectory not yet supported");
	return Result::Success;
}

FileDescriptor TWLPhotoArchive::openFile(const FSPath& path, const FilePerms& perms) {
	Helpers::panic("[TWL_PHOTO] OpenFile not yet supported");
	return FileError;
}

Rust::Result<ArchiveBase*, HorizonResult> TWLPhotoArchive::openArchive(const FSPath& path) {
	if (!path.isEmptyType()) {
		Helpers::panic("Unimplemented path type for TWLPhotoArchive::OpenArchive");
	}

	Helpers::warn("Unimplemented: TWL_PHOTO archive");
	return Err(Result::FailurePlaceholder);
}

Rust::Result<DirectorySession, HorizonResult> TWLPhotoArchive::openDirectory(const FSPath& path) {
	Helpers::panic("[TWL_PHOTO] OpenDirectory not yet supported");
	return Err(Result::FailurePlaceholder);
}
