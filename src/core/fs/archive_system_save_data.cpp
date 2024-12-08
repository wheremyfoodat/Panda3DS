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

HorizonResult SystemSaveDataArchive::createDirectory(const FSPath& path) {
	if (path.type == PathType::UTF16) {
		if (!isPathSafe<PathType::UTF16>(path)) {
			Helpers::panic("Unsafe path in SystemSaveData::CreateDirectory");
		}

		fs::path p = IOFile::getAppData() / ".." / "SharedFiles" / "SystemSaveData";
		p += fs::path(path.utf16_string).make_preferred();

		if (fs::is_directory(p)) {
			return Result::FS::AlreadyExists;
		}

		if (fs::is_regular_file(p)) {
			Helpers::panic("File path passed to SystemSaveData::CreateDirectory");
		}

		bool success = fs::create_directory(p);
		return success ? Result::Success : Result::FS::UnexpectedFileOrDir;
	} else {
		Helpers::panic("Unimplemented SystemSaveData::CreateDirectory");
	}
}


HorizonResult SystemSaveDataArchive::deleteFile(const FSPath& path) {
	if (path.type == PathType::UTF16) {
		if (!isPathSafe<PathType::UTF16>(path)) {
			Helpers::panic("Unsafe path in SystemSaveData::DeleteFile");
		}

		fs::path p = IOFile::getAppData() / ".." / "SharedFiles" / "SystemSaveData";
		p += fs::path(path.utf16_string).make_preferred();

		if (fs::is_directory(p)) {
			Helpers::panic("SystemSaveData::DeleteFile: Tried to delete directory");
		}

		if (!fs::is_regular_file(p)) {
			return Result::FS::FileNotFoundAlt;
		}

		std::error_code ec;
		bool success = fs::remove(p, ec);

		// It might still be possible for fs::remove to fail, if there's eg an open handle to a file being deleted
		// In this case, print a warning, but still return success for now
		if (!success) {
			Helpers::warn("SystemSaveData::DeleteFile: fs::remove failed\n");
		}

		return Result::Success;
	}

	Helpers::panic("SystemSaveData::DeleteFile: Unknown path type");
	return Result::Success;
}

Rust::Result<DirectorySession, HorizonResult> SystemSaveDataArchive::openDirectory(const FSPath& path) {
	if (path.type == PathType::UTF16) {
		if (!isPathSafe<PathType::UTF16>(path)) {
			Helpers::warn("Unsafe path in SystemSaveData::OpenDirectory");
			return Err(Result::FS::FileNotFoundAlt);
		}

		fs::path p = IOFile::getAppData() / ".." / "SharedFiles" / "SystemSaveData";
		p += fs::path(path.utf16_string).make_preferred();

		if (fs::is_regular_file(p)) {
			printf("SystemSaveData: OpenDirectory used with a file path");
			return Err(Result::FS::UnexpectedFileOrDir);
		}

		if (fs::is_directory(p)) {
			return Ok(DirectorySession(this, p));
		} else {
			return Err(Result::FS::FileNotFoundAlt);
		}
	}

	Helpers::panic("SystemSaveData::OpenDirectory: Unimplemented path type");
	return Err(Result::Success);
}