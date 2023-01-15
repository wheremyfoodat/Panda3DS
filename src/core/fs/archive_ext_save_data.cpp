#include "fs/archive_ext_save_data.hpp"
#include <memory>

namespace fs = std::filesystem;

bool ExtSaveDataArchive::openFile(const FSPath& path) {
	if (path.type == PathType::UTF16) {
		if (!isPathSafe<PathType::UTF16>(path))
			Helpers::panic("Unsafe path in ExtSaveData::OpenFile");

		fs::path p = IOFile::getAppData() / "NAND";
		p += fs::path(path.utf16_string).make_preferred();
		return false;
	}

	Helpers::panic("ExtSaveDataArchive::OpenFile: Failed");
	return false;
}

ArchiveBase* ExtSaveDataArchive::openArchive(const FSPath& path) {
	if (path.type != PathType::Binary || path.binary.size() != 12) {
		Helpers::panic("ExtSaveData accessed with an invalid path in OpenArchive");
	}

	return this;
}

std::optional<u32> ExtSaveDataArchive::readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) {
	Helpers::panic("ExtSaveDataArchive::ReadFile: Failed");
	return std::nullopt;
}