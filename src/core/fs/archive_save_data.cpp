#include "fs/archive_save_data.hpp"
#include <algorithm>
#include <memory>

namespace fs = std::filesystem;

FSResult SaveDataArchive::createFile(const FSPath& path, u64 size) {
	Helpers::panic("[SaveData] CreateFile not yet supported");
	return FSResult::Success;
}

FSResult SaveDataArchive::deleteFile(const FSPath& path) {
	Helpers::panic("[SaveData] Unimplemented DeleteFile");
	return FSResult::Success;
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

Rust::Result<DirectorySession, FSResult> SaveDataArchive::openDirectory(const FSPath& path) {
	if (!cartHasSaveData()) {
		printf("Tried to open SaveData directory without save data\n");
		return Err(FSResult::Success);
	}

	if (path.type == PathType::UTF16) {
		if (!isPathSafe<PathType::UTF16>(path))
			Helpers::panic("Unsafe path in SaveData::OpenDirectory");

		fs::path p = IOFile::getAppData() / "SaveData";
		p += fs::path(path.utf16_string).make_preferred();

		if (fs::is_regular_file(p)) {
			printf("SaveData: OpenArchive used with a file path");
			return Err(FSResult::UnexpectedFileOrDir);
		}

		if (fs::is_directory(p)) {
			return Ok(DirectorySession(this, p));
		} else {
			return Err(FSResult::FileNotFound);
		}
	}

	Helpers::panic("SaveDataArchive::OpenDirectory: Unimplemented path type");
	return Err(FSResult::Success);
}

ArchiveBase::FormatInfo SaveDataArchive::getFormatInfo(const FSPath& path) {
	Helpers::panic("Unimplemented SaveData::GetFormatInfo");
	return FormatInfo{ .size = 0, .numOfDirectories = 255, .numOfFiles = 255, .duplicateData = false };
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