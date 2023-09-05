#include "fs/archive_ext_save_data.hpp"
#include <memory>

namespace fs = std::filesystem;

HorizonResult ExtSaveDataArchive::createFile(const FSPath& path, u64 size) {
	if (size == 0)
		Helpers::panic("ExtSaveData file does not support size == 0");

	if (path.type == PathType::UTF16) {
		if (!isPathSafe<PathType::UTF16>(path))
			Helpers::panic("Unsafe path in ExtSaveData::CreateFile");

		fs::path p = IOFile::getAppData() / backingFolder;
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

		fs::path p = IOFile::getAppData() / backingFolder;
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

		fs::path p = IOFile::getAppData() / backingFolder;
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

HorizonResult ExtSaveDataArchive::createDirectory(const FSPath& path) {
	if (path.type == PathType::UTF16) {
		if (!isPathSafe<PathType::UTF16>(path)) {
			Helpers::panic("Unsafe path in ExtSaveData::OpenFile");
		}

		fs::path p = IOFile::getAppData() / backingFolder;
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

std::string ExtSaveDataArchive::getExtSaveDataPathFromBinary(const FSPath& path) {
	// TODO: Remove punning here
	const u32 mediaType = *(u32*)&path.binary[0];
	const u32 saveLow = *(u32*)&path.binary[4];
	const u32 saveHigh = *(u32*)&path.binary[8];

	// TODO: Should the media type be used here
	return backingFolder + std::to_string(saveLow) + std::to_string(saveHigh);
}

Rust::Result<ArchiveBase*, HorizonResult> ExtSaveDataArchive::openArchive(const FSPath& path) {
	if (path.type != PathType::Binary || path.binary.size() != 12) {
		Helpers::panic("ExtSaveData accessed with an invalid path in OpenArchive");
	}

	// TODO: Readd the format check. I didn't manage to fix it sadly
	// Create a format info path in the style of AppData/FormatInfo/Cartridge10390390194.format
	// fs::path formatInfopath = IOFile::getAppData() / "FormatInfo" / (getExtSaveDataPathFromBinary(path) + ".format");
	// Format info not found so the archive is not formatted
	// if (!fs::is_regular_file(formatInfopath)) {
	//	return isShared ? Err(Result::FS::NotFormatted) : Err(Result::FS::NotFoundInvalid);
	//}

	return Ok((ArchiveBase*)this);
}

Rust::Result<DirectorySession, HorizonResult> ExtSaveDataArchive::openDirectory(const FSPath& path) {
	if (path.type == PathType::UTF16) {
		if (!isPathSafe<PathType::UTF16>(path))
			Helpers::panic("Unsafe path in ExtSaveData::OpenDirectory");

		fs::path p = IOFile::getAppData() / backingFolder;
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