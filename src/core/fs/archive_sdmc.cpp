#include "fs/archive_sdmc.hpp"
#include <memory>

namespace fs = std::filesystem;

HorizonResult SDMCArchive::createFile(const FSPath& path, u64 size) {
	if (path.type == PathType::UTF16) {
		if (!isPathSafe<PathType::UTF16>(path)) {
			Helpers::panic("Unsafe path in SDMC::CreateFile");
		}

		fs::path p = IOFile::getAppData() / "SDMC";
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

	Helpers::panic("SDMC::CreateFile: Failed");
	return Result::Success;
}

HorizonResult SDMCArchive::deleteFile(const FSPath& path) {
	Helpers::panic("[SDMC] Unimplemented DeleteFile");
	return Result::Success;
}

FileDescriptor SDMCArchive::openFile(const FSPath& path, const FilePerms& perms) {
	FilePerms realPerms = perms;
	// SD card always has read permission
	realPerms.raw |= (1 << 0);

	if ((realPerms.create() && !realPerms.write())) {
		Helpers::panic("[SDMC] Unsupported flags for OpenFile");
	}

	std::filesystem::path p = IOFile::getAppData() / "SDMC";

	switch (path.type) {
		case PathType::ASCII:
			if (!isPathSafe<PathType::ASCII>(path)) {
				Helpers::panic("Unsafe path in SDMCArchive::OpenFile");
			}

			p += fs::path(path.string).make_preferred();
			break;

		case PathType::UTF16:
			if (!isPathSafe<PathType::UTF16>(path)) {
				Helpers::panic("Unsafe path in SDMCArchive::OpenFile");
			}

			p += fs::path(path.utf16_string).make_preferred();
			break;

		default: Helpers::panic("SDMCArchive::OpenFile: Failed. Path type: %d", path.type); return FileError;
	}

	const char* permString = perms.write() ? "r+b" : "rb";

	if (fs::exists(p)) {  // Return file descriptor if the file exists
		IOFile file(p.string().c_str(), permString);
		return file.isOpen() ? file.getHandle() : FileError;
	} else {
		// If the file is not found, create it if the create flag is on
		if (realPerms.create()) {
			IOFile file(p.string().c_str(), "wb");  // Create file
			file.close();                           // Close it

			file.open(p.string().c_str(), permString);  // Reopen with proper perms
			return file.isOpen() ? file.getHandle() : FileError;
		} else {
			return FileError;
		}
	}
}

HorizonResult SDMCArchive::createDirectory(const FSPath& path) {
	std::filesystem::path p = IOFile::getAppData() / "SDMC";

	switch (path.type) {
		case PathType::ASCII:
			if (!isPathSafe<PathType::ASCII>(path)) {
				Helpers::panic("Unsafe path in SDMCArchive::OpenFile");
			}

			p += fs::path(path.string).make_preferred();
			break;

		case PathType::UTF16:
			if (!isPathSafe<PathType::UTF16>(path)) {
				Helpers::panic("Unsafe path in SDMCArchive::OpenFile");
			}

			p += fs::path(path.utf16_string).make_preferred();
			break;

		default: Helpers::panic("SDMCArchive::CreateDirectory: Failed. Path type: %d", path.type); return Result::FailurePlaceholder;
	}

	if (fs::is_directory(p)) {
		return Result::FS::AlreadyExists;
	}

	if (fs::is_regular_file(p)) {
		Helpers::panic("File path passed to SDMCArchive::CreateDirectory");
	}

	std::error_code ec;
	bool success = fs::create_directory(p, ec);
	return success ? Result::Success : Result::FS::UnexpectedFileOrDir;
}

Rust::Result<DirectorySession, HorizonResult> SDMCArchive::openDirectory(const FSPath& path) {
	if (path.type == PathType::UTF16) {
		if (!isPathSafe<PathType::UTF16>(path)) {
			Helpers::panic("Unsafe path in SaveData::OpenDirectory");
		}

		fs::path p = IOFile::getAppData() / "SDMC";
		p += fs::path(path.utf16_string).make_preferred();

		if (fs::is_regular_file(p)) {
			printf("SDMC: OpenDirectory used with a file path");
			return Err(Result::FS::UnexpectedFileOrDir);
		}

		if (fs::is_directory(p)) {
			return Ok(DirectorySession(this, p));
		} else {
			return Err(Result::FS::FileNotFoundAlt);
		}
	}

	Helpers::panic("SDMCArchive::OpenDirectory: Unimplemented path type");
	return Err(Result::Success);
}

Rust::Result<ArchiveBase*, HorizonResult> SDMCArchive::openArchive(const FSPath& path) {
	// TODO: Fail here if the SD is disabled in the connfig.
	if (path.type != PathType::Empty) {
		Helpers::panic("Unimplemented path type for SDMC::OpenArchive");
	}

	return Ok((ArchiveBase*)this);
}

std::optional<u32> SDMCArchive::readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) {
	printf("SDMCArchive::ReadFile: Failed\n");
	return std::nullopt;
}