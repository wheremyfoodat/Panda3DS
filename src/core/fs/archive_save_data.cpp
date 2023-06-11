#include "fs/archive_save_data.hpp"
#include <algorithm>
#include <memory>

namespace fs = std::filesystem;

FSResult SaveDataArchive::createFile(const FSPath& path, u64 size) {
	if (size == 0)
		Helpers::panic("SaveData file does not support size == 0");

	if (path.type == PathType::UTF16) {
		if (!isPathSafe<PathType::UTF16>(path))
			Helpers::panic("Unsafe path in SaveData::CreateFile");

		fs::path p = IOFile::getAppData() / "SaveData";
		p += fs::path(path.utf16_string).make_preferred();

		if (fs::exists(p))
			return FSResult::AlreadyExists;
			
		// Create a file of size "size" by creating an empty one, seeking to size - 1 and just writing a 0 there
		IOFile file(p.string().c_str(), "wb");
		if (file.seek(size - 1, SEEK_SET) && file.writeBytes("", 1).second == 1) {
			return FSResult::Success;
		}

		return FSResult::FileTooLarge;
	}

	Helpers::panic("SaveDataArchive::OpenFile: Failed");
	return FSResult::Success;
}

FSResult SaveDataArchive::createDirectory(const FSPath& path) {
	if (path.type == PathType::UTF16) {
		if (!isPathSafe<PathType::UTF16>(path))
			Helpers::panic("Unsafe path in SaveData::OpenFile");

		fs::path p = IOFile::getAppData() / "SaveData";
		p += fs::path(path.utf16_string).make_preferred();

		if (fs::is_directory(p))
			return FSResult::AlreadyExists;
		if (fs::is_regular_file(p)) {
			Helpers::panic("File path passed to SaveData::CreateDirectory");
		}

		bool success = fs::create_directory(p);
		return success ? FSResult::Success : FSResult::UnexpectedFileOrDir;
	} else {
		Helpers::panic("Unimplemented SaveData::CreateDirectory");
	}
}

FSResult SaveDataArchive::deleteFile(const FSPath& path) {
	if (path.type == PathType::UTF16) {
		if (!isPathSafe<PathType::UTF16>(path))
			Helpers::panic("Unsafe path in SaveData::DeleteFile");

		fs::path p = IOFile::getAppData() / "SaveData";
		p += fs::path(path.utf16_string).make_preferred();

		if (fs::is_directory(p)) {
			Helpers::panic("SaveData::DeleteFile: Tried to delete directory");
		}

		if (!fs::is_regular_file(p)) {
			return FSResult::FileNotFound;
		}

		std::error_code ec;
		bool success = fs::remove(p, ec);

		// It might still be possible for fs::remove to fail, if there's eg an open handle to a file being deleted
		// In this case, print a warning, but still return success for now
		if (!success) {
			Helpers::warn("SaveData::DeleteFile: fs::remove failed\n");
		}

		return FSResult::Success;
	}

	Helpers::panic("SaveDataArchive::DeleteFile: Unknown path type");
	return FSResult::Success;
}

FileDescriptor SaveDataArchive::openFile(const FSPath& path, const FilePerms& perms) {
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
	if (path.type == PathType::UTF16) {
		if (!isPathSafe<PathType::UTF16>(path))
			Helpers::panic("Unsafe path in SaveData::OpenDirectory");

		fs::path p = IOFile::getAppData() / "SaveData";
		p += fs::path(path.utf16_string).make_preferred();

		if (fs::is_regular_file(p)) {
			printf("SaveData: OpenDirectory used with a file path");
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

Rust::Result<ArchiveBase::FormatInfo, FSResult> SaveDataArchive::getFormatInfo(const FSPath& path) {
	const fs::path formatInfoPath = getFormatInfoPath();
	IOFile file(formatInfoPath, "rb");

	// If the file failed to open somehow, we return that the archive is not formatted
	if (!file.isOpen()) {
		return Err(FSResult::NotFormatted);
	}

	FormatInfo ret;
	auto [success, bytesRead] = file.readBytes(&ret, sizeof(FormatInfo));
	if (!success || bytesRead != sizeof(FormatInfo)) {
		Helpers::warn("SaveData::GetFormatInfo: Format file exists but was not properly read into the FormatInfo struct");
		return Err(FSResult::NotFormatted);
	}

	return Ok(ret);
}

void SaveDataArchive::format(const FSPath& path, const ArchiveBase::FormatInfo& info) {
	const fs::path saveDataPath = IOFile::getAppData() / "SaveData";
	const fs::path formatInfoPath = getFormatInfoPath();

	// Delete all contents by deleting the directory then recreating it 
	fs::remove_all(saveDataPath);
	fs::create_directories(saveDataPath);

	// Write format info on disk
	IOFile file(formatInfoPath, "wb");
	file.writeBytes(&info, sizeof(info));
}

Rust::Result<ArchiveBase*, FSResult> SaveDataArchive::openArchive(const FSPath& path) {
	if (path.type != PathType::Empty) {
		Helpers::panic("Unimplemented path type for SaveData archive: %d\n", path.type);
		return Err(FSResult::NotFoundInvalid);
	}

	const fs::path formatInfoPath = getFormatInfoPath();
	// Format info not found so the archive is not formatted
	if (!fs::is_regular_file(formatInfoPath)) {
		return Err(FSResult::NotFormatted);
	}

	return Ok((ArchiveBase*)this);
}

std::optional<u32> SaveDataArchive::readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) {
	Helpers::panic("Unimplemented SaveData::ReadFile");
	return 0;
}