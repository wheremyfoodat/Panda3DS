#include "fs/archive_sdmc.hpp"
#include <memory>

HorizonResult SDMCArchive::createFile(const FSPath& path, u64 size) {
	Helpers::panic("[SDMC] CreateFile not yet supported");
	return Result::Success;
}

HorizonResult SDMCArchive::deleteFile(const FSPath& path) {
	Helpers::panic("[SDMC] Unimplemented DeleteFile");
	return Result::Success;
}

FileDescriptor SDMCArchive::openFile(const FSPath& path, const FilePerms& perms) {
	printf("SDMCArchive::OpenFile: Failed");
	return FileError;
}

Rust::Result<ArchiveBase*, HorizonResult> SDMCArchive::openArchive(const FSPath& path) {
	printf("SDMCArchive::OpenArchive: Failed\n");
	return Err(Result::FS::NotFormatted);
}

std::optional<u32> SDMCArchive::readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) {
	printf("SDMCArchive::ReadFile: Failed\n");
	return std::nullopt;
}