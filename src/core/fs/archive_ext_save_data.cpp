#include "fs/archive_ext_save_data.hpp"
#include <memory>

namespace fs = std::filesystem;

CreateFileResult ExtSaveDataArchive::createFile(const FSPath& path, u64 size) {
	if (size == 0)
		Helpers::panic("ExtSaveData file does not support size == 0");

	if (path.type == PathType::UTF16) {
		if (!isPathSafe<PathType::UTF16>(path))
			Helpers::panic("Unsafe path in ExtSaveData::CreateFile");

		fs::path p = IOFile::getAppData() / "NAND";
		p += fs::path(path.utf16_string).make_preferred();

		if (fs::exists(p))
			return CreateFileResult::AlreadyExists;
			
		// Create a file of size "size" by creating an empty one, seeking to size - 1 and just writing a 0 there
		IOFile file(p.string().c_str(), "wb");
		if (file.seek(size - 1, SEEK_SET) && file.writeBytes("", 1).second == 1) {
			return CreateFileResult::Success;
		}

		return CreateFileResult::FileTooLarge;
	}

	Helpers::panic("ExtSaveDataArchive::OpenFile: Failed");
	return CreateFileResult::Success;
}

DeleteFileResult ExtSaveDataArchive::deleteFile(const FSPath& path) {
	if (path.type == PathType::UTF16) {
		if (!isPathSafe<PathType::UTF16>(path))
			Helpers::panic("Unsafe path in ExtSaveData::DeleteFile");

		fs::path p = IOFile::getAppData() / "NAND";
		p += fs::path(path.utf16_string).make_preferred();

		std::error_code ec;
		bool success = fs::remove(p, ec);
		return success ? DeleteFileResult::Success : DeleteFileResult::FileNotFound;
	}

	Helpers::panic("ExtSaveDataArchive::DeleteFile: Failed");
	return DeleteFileResult::Success;
}

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

	if (path.binary[0] != 0) Helpers::panic("ExtSaveData: Tried to access something other than NAND. ID: %02X", path.binary[0]);

	return this;
}

std::optional<u32> ExtSaveDataArchive::readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) {
	Helpers::panic("ExtSaveDataArchive::ReadFile: Failed");
	return std::nullopt;
}