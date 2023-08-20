#include "fs/archive_sdmc.hpp"
#include <memory>

namespace fs = std::filesystem;

HorizonResult SDMCArchive::createFile(const FSPath& path, u64 size) {
	Helpers::panic("[SDMC] CreateFile not yet supported");
	return Result::Success;
}

HorizonResult SDMCArchive::deleteFile(const FSPath& path) {
	Helpers::panic("[SDMC] Unimplemented DeleteFile");
	return Result::Success;
}

FileDescriptor SDMCArchive::openFile(const FSPath& path, const FilePerms& perms) {
	if (path.type == PathType::ASCII) {
		if (!isPathSafe<PathType::ASCII>(path)) {
			Helpers::panic("Unsafe path in SaveData::OpenFile");
		}

		FilePerms realPerms = perms;
		// SD card always has read permission
		realPerms.raw |= (1 << 0);

		if ((realPerms.create() && !realPerms.write())) {
			Helpers::panic("[SaveData] Unsupported flags for OpenFile");
		}

		fs::path p = IOFile::getAppData() / "SDMC";
		p += fs::path(path.string).make_preferred();

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

	Helpers::panic("SDMCArchive::OpenFile: Failed");
	return FileError;
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