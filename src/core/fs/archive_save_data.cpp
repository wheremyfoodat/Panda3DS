#include "fs/archive_save_data.hpp"
#include <memory>

bool SaveDataArchive::openFile(const FSPath& path) {
	Helpers::panic("SaveDataArchive::OpenFile");
	return false;
}

ArchiveBase* SaveDataArchive::openArchive(const FSPath& path) {
	Helpers::panic("SaveDataArchive::OpenArchive");
	return nullptr;
}

std::optional<u32> SaveDataArchive::readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) {
	Helpers::panic("SaveDataArchive::ReadFile");
	return std::nullopt;
}