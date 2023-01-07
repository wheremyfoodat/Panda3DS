#include "fs/archive_ext_save_data.hpp"
#include <memory>

bool ExtSaveDataArchive::openFile(const FSPath& path) {
	Helpers::panic("ExtSaveDataArchive::OpenFile: Failed");
	return false;
}

ArchiveBase* ExtSaveDataArchive::openArchive(const FSPath& path) {
	Helpers::panic("ExtSaveDataArchive::OpenArchive: Failed. Path type: %d", path.type);
	return nullptr;
}

std::optional<u32> ExtSaveDataArchive::readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) {
	Helpers::panic("ExtSaveDataArchive::ReadFile: Failed");
	return std::nullopt;
}