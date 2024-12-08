#include <algorithm>
#include <memory>

#include "fs/archive_twl_sound.hpp"

namespace fs = std::filesystem;

HorizonResult TWLSoundArchive::createFile(const FSPath& path, u64 size) {
	Helpers::panic("[TWL_SOUND] CreateFile not yet supported");
	return Result::Success;
}

HorizonResult TWLSoundArchive::deleteFile(const FSPath& path) {
	Helpers::panic("[TWL_SOUND] Unimplemented DeleteFile");
	return Result::Success;
}

HorizonResult TWLSoundArchive::createDirectory(const FSPath& path) {
	Helpers::panic("[TWL_SOUND] CreateDirectory not yet supported");
	return Result::Success;
}

FileDescriptor TWLSoundArchive::openFile(const FSPath& path, const FilePerms& perms) {
	Helpers::panic("[TWL_SOUND] OpenFile not yet supported");
	return FileError;
}

Rust::Result<ArchiveBase*, HorizonResult> TWLSoundArchive::openArchive(const FSPath& path) {
	if (path.type != PathType::Empty) {
		Helpers::panic("Unimplemented path type for TWLSoundArchive::OpenArchive");
	}

	Helpers::warn("Unimplemented: TWL_SOUND archive");
	return Err(Result::FailurePlaceholder);
}

Rust::Result<DirectorySession, HorizonResult> TWLSoundArchive::openDirectory(const FSPath& path) {
	Helpers::panic("[TWL_SOUND] OpenDirectory not yet supported");
	return Err(Result::FailurePlaceholder);
}
