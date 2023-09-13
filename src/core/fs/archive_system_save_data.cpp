#include <algorithm>
#include "fs/archive_system_save_data.hpp"

namespace fs = std::filesystem;

Rust::Result<ArchiveBase*, HorizonResult> SystemSaveDataArchive::openArchive(const FSPath& path) {
	if (path.type != PathType::Binary) {
		Helpers::panic("Unimplemented path type for SystemSaveData::OpenArchive");
	}

	return Ok((ArchiveBase*)this);
}

FileDescriptor SystemSaveDataArchive::openFile(const FSPath& path, const FilePerms& perms) {
	// TODO: Validate this. Temporarily copied from SaveData archive

	if (path.type == PathType::UTF16) {
		if (!isPathSafe<PathType::UTF16>(path)) {
			Helpers::panic("Unsafe path in SystemSaveData::OpenFile");
		}

		if (perms.raw == 0 || (perms.create() && !perms.write())) {
			Helpers::panic("[SystemSaveData] Unsupported flags for OpenFile");
		}

		fs::path p = IOFile::getAppData() / ".." / "SharedFiles" / "SystemSaveData";
		p += fs::path(path.utf16_string).make_preferred();

		const char* permString = perms.write() ? "r+b" : "rb";

		if (fs::exists(p)) {  // Return file descriptor if the file exists
			IOFile file(p.string().c_str(), permString);
			return file.isOpen() ? file.getHandle() : FileError;
		} else {
			// If the file is not found, create it if the create flag is on
			if (perms.create()) {
				IOFile file(p.string().c_str(), "wb");  // Create file
				file.close();                           // Close it

				file.open(p.string().c_str(), permString);  // Reopen with proper perms
				return file.isOpen() ? file.getHandle() : FileError;
			} else {
				return FileError;
			}
		}
	}

	Helpers::panic("SystemSaveData::OpenFile: Failed");
	return FileError;
}

HorizonResult SystemSaveDataArchive::createFile(const FSPath& path, u64 size) {
	if (path.type == PathType::UTF16) {
		if (!isPathSafe<PathType::UTF16>(path)) {
			Helpers::panic("Unsafe path in SystemSaveData::CreateFile");
		}

		fs::path p = IOFile::getAppData() / ".." / "SharedFiles" / "SystemSaveData";
		p += fs::path(path.utf16_string).make_preferred();

		if (fs::exists(p)) {
			return Result::FS::AlreadyExists;
		}

		IOFile file(p.string().c_str(), "wb");

		// If the size is 0, leave the file empty and return success
		if (size == 0) {
			file.close();
			return Result::Success;
		}

		// If it is not empty, seek to size - 1 and write a 0 to create a file of size "size"
		else if (file.seek(size - 1, SEEK_SET) && file.writeBytes("", 1).second == 1) {
			file.close();
			return Result::Success;
		}

		file.close();
		return Result::FS::FileTooLarge;
	}

	Helpers::panic("SystemSaveData::CreateFile: Failed");
	return Result::Success;
}