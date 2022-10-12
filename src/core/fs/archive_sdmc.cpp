#include "fs/archive_sdmc.hpp"
#include <memory>

bool SDMCArchive::openFile(const FSPath& path) {
	printf("SDMCArchive::OpenFile: Failed");
	return false;
}

ArchiveBase* SDMCArchive::openArchive(const FSPath& path) {
	printf("SDMCArchive::OpenArchive: Failed");
	return nullptr;
}

std::optional<u32> SDMCArchive::readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) {
	printf("SDMCArchive::ReadFile: Failed");
	return std::nullopt;
}