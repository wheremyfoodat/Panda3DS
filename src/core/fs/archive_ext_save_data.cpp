#include "fs/archive_ext_save_data.hpp"
#include <memory>

namespace fs = std::filesystem;

FileDescriptor ExtSaveDataArchive::openFile(const FSPath& path, const FilePerms& perms) {
	if (path.type == PathType::UTF16) {
		if (!isPathSafe<PathType::UTF16>(path))
			Helpers::panic("Unsafe path in ExtSaveData::OpenFile");

		if (perms.create())
			Helpers::panic("[ExtSaveData] Can't open file with create flag");

		fs::path p = IOFile::getAppData() / "NAND";
		p += fs::path(path.utf16_string).make_preferred();

		if (fs::exists(p)) { // Return file descriptor if the file exists
			IOFile file(p.string().c_str(), "r+b"); // According to Citra, this ignores the OpenFlags field and always opens as r+b? TODO: Check
			return file.isOpen() ? file.getHandle() : FileError;
		} else {
			return FileError;
		}
	}

	Helpers::panic("ExtSaveDataArchive::OpenFile: Failed");
	return FileError;
}

ArchiveBase* ExtSaveDataArchive::openArchive(const FSPath& path) {
	if (path.type != PathType::Binary || path.binary.size() != 12) {
		Helpers::panic("ExtSaveData accessed with an invalid path in OpenArchive");
	}

	if (path.binary[0] != 0) Helpers::panic("ExtSaveData: Tried to access something other than NAND");

	return this;
}

std::optional<u32> ExtSaveDataArchive::readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) {
	Helpers::panic("ExtSaveDataArchive::ReadFile: Failed");
	return std::nullopt;
}