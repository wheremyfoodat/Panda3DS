#include "fs/archive_ncch.hpp"
#include <memory>

CreateFileResult NCCHArchive::createFile(const FSPath& path, u64 size) {
	Helpers::panic("[NCCH] CreateFile not yet supported");
	return CreateFileResult::Success;
}

FileDescriptor NCCHArchive::openFile(const FSPath& path, const FilePerms& perms) {
	Helpers::panic("NCCHArchive::OpenFile: Unimplemented");
	return FileError;
}

ArchiveBase* NCCHArchive::openArchive(const FSPath& path) {
	Helpers::panic("NCCHArchive::OpenArchive: Unimplemented");
	return nullptr;
}

std::optional<u32> NCCHArchive::readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) {
	Helpers::panic("NCCHArchive::ReadFile: Unimplemented");
	return std::nullopt;
}