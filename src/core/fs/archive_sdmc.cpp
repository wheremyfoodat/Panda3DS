#include "fs/archive_sdmc.hpp"
#include <memory>

CreateFileResult SDMCArchive::createFile(const FSPath& path, u64 size) {
	Helpers::panic("[SDMC] CreateFile not yet supported");
	return CreateFileResult::Success;
}

DeleteFileResult SDMCArchive::deleteFile(const FSPath& path) {
	Helpers::panic("[SDMC] Unimplemented DeleteFile");
	return DeleteFileResult::Success;
}

FileDescriptor SDMCArchive::openFile(const FSPath& path, const FilePerms& perms) {
	printf("SDMCArchive::OpenFile: Failed");
	return FileError;
}

ArchiveBase* SDMCArchive::openArchive(const FSPath& path) {
	printf("SDMCArchive::OpenArchive: Failed\n");
	return nullptr;
}

std::optional<u32> SDMCArchive::readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) {
	printf("SDMCArchive::ReadFile: Failed\n");
	return std::nullopt;
}