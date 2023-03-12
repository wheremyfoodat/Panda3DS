#include "fs/archive_save_data.hpp"
#include <algorithm>
#include <memory>

namespace fs = std::filesystem;

CreateFileResult SaveDataArchive::createFile(const FSPath& path, u64 size) {
	Helpers::panic("[SaveData] CreateFile not yet supported");
	return CreateFileResult::Success;
}

DeleteFileResult SaveDataArchive::deleteFile(const FSPath& path) {
	Helpers::panic("[SaveData] Unimplemented DeleteFile");
	return DeleteFileResult::Success;
}

FileDescriptor SaveDataArchive::openFile(const FSPath& path, const FilePerms& perms) {
	if (!cartHasSaveData()) {
		printf("Tried to read SaveData FS without save data\n");
		return FileError;
	}

	if (path.type == PathType::UTF16) {
		if (!isPathSafe<PathType::UTF16>(path))
			Helpers::panic("Unsafe path in SaveData::OpenFile");

		if (perms.raw == 0 || (perms.create() && !perms.write()))
			Helpers::panic("[SaveData] Unsupported flags for OpenFile");

		fs::path p = IOFile::getAppData() / "SaveData";
		p += fs::path(path.utf16_string).make_preferred();

		const char* permString = perms.write() ? "r+b" : "rb";

		if (fs::exists(p)) { // Return file descriptor if the file exists
			IOFile file(p.string().c_str(), permString);
			return file.isOpen() ? file.getHandle() : FileError;
		} else {
			// If the file is not found, create it if the create flag is on
			if (perms.create()) {
				IOFile file(p.string().c_str(), "wb"); // Create file
				file.close(); // Close it

				file.open(p.string().c_str(), permString); // Reopen with proper perms
				return file.isOpen() ? file.getHandle() : FileError;
			} else {
				return FileError;
			}
		}
	}

	Helpers::panic("SaveDataArchive::OpenFile: Failed");
	return FileError;
}

ArchiveBase* SaveDataArchive::openArchive(const FSPath& path) {
	if (path.type != PathType::Empty) {
		Helpers::panic("Unimplemented path type for SaveData archive: %d\n", path.type);
		return nullptr;
	}

	return this;
}

std::optional<u32> SaveDataArchive::readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) {
	if (!cartHasSaveData()) {
		printf("Tried to read SaveData FS without save data\n");
		return std::nullopt;
	}

	auto cxi = mem.getCXI();
	const u64 saveSize = cxi->saveData.size();

	if (offset >= saveSize) {
		printf("Tried to read from past the end of save data\n");
		return std::nullopt;
	}

	Helpers::panic("Unimplemented SaveData::ReadFile");
	return 0;
}