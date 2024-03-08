#include "fs/archive_ext_save_data.hpp"
#include <memory>

namespace fs = std::filesystem;

HorizonResult ExtSaveDataArchive::createFile(const FSPath& path, u64 size) {
	if (size == 0)
		Helpers::panic("ExtSaveData file does not support size == 0");

	if (path.type == PathType::UTF16) {
		if (!isPathSafe<PathType::UTF16>(path))
			Helpers::panic("Unsafe path in ExtSaveData::CreateFile");

		fs::path p = getUserDataPath();

		p += fs::path(path.utf16_string).make_preferred();

		if (fs::exists(p))
			return Result::FS::AlreadyExists;

		// Create a file of size "size" by creating an empty one, seeking to size - 1 and just writing a 0 there
		IOFile file(p.string().c_str(), "wb");
		if (file.seek(size - 1, SEEK_SET) && file.writeBytes("", 1).second == 1) {
			file.close();
			return Result::Success;
		}

		file.close();
		return Result::FS::FileTooLarge;
	}

	Helpers::panic("ExtSaveDataArchive::OpenFile: Failed");
	return Result::Success;
}

HorizonResult ExtSaveDataArchive::deleteFile(const FSPath& path) {
	if (path.type == PathType::UTF16) {
		if (!isPathSafe<PathType::UTF16>(path))
			Helpers::panic("Unsafe path in ExtSaveData::DeleteFile");

		fs::path p = getUserDataPath();

		p += fs::path(path.utf16_string).make_preferred();

		if (fs::is_directory(p)) {
			Helpers::panic("ExtSaveData::DeleteFile: Tried to delete directory");
		}

		if (!fs::is_regular_file(p)) {
			return Result::FS::FileNotFoundAlt;
		}

		std::error_code ec;
		bool success = fs::remove(p, ec);

		// It might still be possible for fs::remove to fail, if there's eg an open handle to a file being deleted
		// In this case, print a warning, but still return success for now
		if (!success) {
			Helpers::warn("ExtSaveData::DeleteFile: fs::remove failed\n");
		}

		return Result::Success;
	}

	Helpers::panic("ExtSaveDataArchive::DeleteFile: Unknown path type");
	return Result::Success;
}

FileDescriptor ExtSaveDataArchive::openFile(const FSPath& path, const FilePerms& perms) {
	if (path.type == PathType::UTF16) {
		if (!isPathSafe<PathType::UTF16>(path))
			Helpers::panic("Unsafe path in ExtSaveData::OpenFile");

		if (perms.create())
			Helpers::panic("[ExtSaveData] Can't open file with create flag");

		fs::path p = getUserDataPath();

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

HorizonResult ExtSaveDataArchive::renameFile(const FSPath& oldPath, const FSPath& newPath) {
	if (oldPath.type != PathType::UTF16 || newPath.type != PathType::UTF16) {
		Helpers::panic("Invalid path type for ExtSaveData::RenameFile");
	}

	if (!isPathSafe<PathType::UTF16>(oldPath) || !isPathSafe<PathType::UTF16>(newPath)) {
		Helpers::panic("Unsafe path in ExtSaveData::RenameFile");
	}

	// Construct host filesystem paths
	fs::path sourcePath = getUserDataPath();

	fs::path destPath = sourcePath;

	sourcePath += fs::path(oldPath.utf16_string).make_preferred();
	destPath += fs::path(newPath.utf16_string).make_preferred();

	if (!fs::is_regular_file(sourcePath) || fs::is_directory(sourcePath)) {
		Helpers::warn("ExtSaveData::RenameFile: Source path is not a file or is directory");
		return Result::FS::RenameNonexistentFileOrDir;
	}

	if (fs::is_regular_file(destPath) || fs::is_directory(destPath)) {
		Helpers::warn("ExtSaveData::RenameFile: Dest path already exists");
		return Result::FS::RenameFileDestExists;
	}

	std::error_code ec;
	fs::rename(sourcePath, destPath, ec);

	if (ec) {
		Helpers::warn("Error in ExtSaveData::RenameFile");
		return Result::FS::RenameNonexistentFileOrDir;
	}

	return Result::Success;
}

HorizonResult ExtSaveDataArchive::createDirectory(const FSPath& path) {
	if (path.type == PathType::UTF16) {
		if (!isPathSafe<PathType::UTF16>(path)) {
			Helpers::panic("Unsafe path in ExtSaveData::OpenFile");
		}

		fs::path p = getUserDataPath();
		p += fs::path(path.utf16_string).make_preferred();

		if (fs::is_directory(p)) return Result::FS::AlreadyExists;
		if (fs::is_regular_file(p)) {
			Helpers::panic("File path passed to ExtSaveData::CreateDirectory");
		}

		bool success = fs::create_directory(p);
		return success ? Result::Success : Result::FS::UnexpectedFileOrDir;
	} else {
		Helpers::panic("Unimplemented ExtSaveData::CreateDirectory");
	}
}

HorizonResult ExtSaveDataArchive::deleteDirectory(const FSPath& path) {
	if (path.type == PathType::UTF16) {
		if (!isPathSafe<PathType::UTF16>(path)) {
			Helpers::panic("Unsafe path in ExtSaveData::DeleteDirectory");
		}

		fs::path p = getUserDataPath();
		p += fs::path(path.utf16_string).make_preferred();

		if (!fs::is_directory(p)) {
			return Result::FS::NotFoundInvalid;
		}

		if (fs::is_regular_file(p)) {
			Helpers::panic("File path passed to ExtSaveData::DeleteDirectory");
		}

		Helpers::warn("Stubbed DeleteDirectory for %s archive", name().c_str());
		bool success = fs::remove(p);
		return success ? Result::Success : Result::FS::UnexpectedFileOrDir;
	} else {
		Helpers::panic("Unimplemented ExtSaveData::DeleteDirectory");
	}
}


HorizonResult ExtSaveDataArchive::deleteDirectoryRecursively(const FSPath& path) {
	if (path.type == PathType::UTF16) {
		if (!isPathSafe<PathType::UTF16>(path)) {
			Helpers::panic("Unsafe path in ExtSaveData::DeleteDirectoryRecursively");
		}

		fs::path p = getUserDataPath();
		p += fs::path(path.utf16_string).make_preferred();

		if (!fs::is_directory(p)) {
			return Result::FS::NotFoundInvalid;
		}

		if (fs::is_regular_file(p)) {
			Helpers::panic("File path passed to ExtSaveData::DeleteDirectoryRecursively");
		}

		Helpers::warn("Stubbed DeleteDirectoryRecursively for %s archive", name().c_str());
		bool success = fs::remove_all(p);
		return success ? Result::Success : Result::FS::UnexpectedFileOrDir;
	} else {
		Helpers::panic("Unimplemented ExtSaveData::DeleteDirectoryRecursively");
	}
}

void ExtSaveDataArchive::format(const FSPath& path, const FormatInfo& info) {
	const fs::path saveDataPath = IOFile::getAppData() / backingFolder;
	const fs::path formatInfoPath = getFormatInfoPath(path);

	// Delete all contents by deleting the directory then recreating it
	fs::remove_all(saveDataPath);
	fs::create_directories(saveDataPath);

	if (!isShared) {
		fs::create_directories(saveDataPath / "user");
		fs::create_directories(saveDataPath / "boss");
		// todo: save icon.
	}

	// Write format info on disk
	IOFile file(formatInfoPath, "wb");
	file.writeBytes(&info, sizeof(info));
	file.flush();
	file.close();
}

void ExtSaveDataArchive::clear(const FSPath& path) const {
	const fs::path saveDataPath = IOFile::getAppData() / backingFolder;
	const fs::path formatInfoPath = getFormatInfoPath(path);

	fs::remove_all(saveDataPath);
	fs::remove(formatInfoPath);
}

std::filesystem::path ExtSaveDataArchive::getFormatInfoPath(const FSPath& path) const {
	return IOFile::getAppData() / "FormatInfo" / (getExtSaveDataPathFromBinary(path) + ".format");
}

std::filesystem::path ExtSaveDataArchive::getUserDataPath() const {
	fs::path p = IOFile::getAppData() / backingFolder;
	if (!isShared) { // todo: "boss"?
		p /= "user";
	}
	return p;
}


Rust::Result<ArchiveBase::FormatInfo, HorizonResult> ExtSaveDataArchive::getFormatInfo(const FSPath& path) {
	const fs::path formatInfoPath = getFormatInfoPath(path);
	IOFile file(formatInfoPath, "rb");

	// If the file failed to open somehow, we return that the archive is not formatted
	if (!file.isOpen()) {
		return Err(Result::FS::NotFormatted);
	}

	FormatInfo ret = {};
	auto [success, bytesRead] = file.readBytes(&ret, sizeof(FormatInfo));
	file.close();

	if (!success || bytesRead != sizeof(FormatInfo)) {
		Helpers::warn("ExtSaveDataArchive::GetFormatInfo: Format file exists but was not properly read into the FormatInfo struct");
		return Err(Result::FS::NotFormatted);
	}

	ret.size = 0;

	return Ok(ret);
}

std::string ExtSaveDataArchive::getExtSaveDataPathFromBinary(const FSPath& path) const {
	if (path.type != PathType::Binary) {
		Helpers::panic("GetExtSaveDataPathFromBinary called without a Binary FSPath!");
	}

	// TODO: Remove punning here
	const u32 mediaType = *(u32*)&path.binary[0];
	const u32 saveLow = *(u32*)&path.binary[4];
	const u32 saveHigh = *(u32*)&path.binary[8];

	// TODO: Should the media type be used here, using it just to be safe.
	return backingFolder + std::to_string(mediaType) + "_" + std::to_string(saveLow) + "_" + std::to_string(saveHigh);
}

Rust::Result<ArchiveBase*, HorizonResult> ExtSaveDataArchive::openArchive(const FSPath& path) {
	if (path.type != PathType::Binary || path.binary.size() != 12) {
		Helpers::panic("ExtSaveData accessed with an invalid path in OpenArchive");
	}

	// Create a format info path in the style of AppData/FormatInfo/Cartridge10390390194.format
	const fs::path formatInfoPath = getFormatInfoPath(path);
	// Format info not found so the archive is not formatted
	if (!fs::is_regular_file(formatInfoPath)) {
		return isShared ? Err(Result::FS::NotFormatted) : Err(Result::FS::NotFoundInvalid);
	}

	return Ok((ArchiveBase*)this);
}

Rust::Result<DirectorySession, HorizonResult> ExtSaveDataArchive::openDirectory(const FSPath& path) {
	if (path.type == PathType::UTF16) {
		if (!isPathSafe<PathType::UTF16>(path))
			Helpers::panic("Unsafe path in ExtSaveData::OpenDirectory");

		fs::path p = getUserDataPath();
		p += fs::path(path.utf16_string).make_preferred();

		if (fs::is_regular_file(p)) {
			printf("ExtSaveData: OpenArchive used with a file path");
			return Err(Result::FS::UnexpectedFileOrDir);
		}

		if (fs::is_directory(p)) {
			return Ok(DirectorySession(this, p));
		} else {
			return Err(Result::FS::FileNotFoundAlt);
		}
	}

	Helpers::panic("ExtSaveDataArchive::OpenDirectory: Unimplemented path type");
	return Err(Result::Success);
}

std::optional<u32> ExtSaveDataArchive::readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) {
	Helpers::panic("ExtSaveDataArchive::ReadFile: Failed");
	return std::nullopt;
}
