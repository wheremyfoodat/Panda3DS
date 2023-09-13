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