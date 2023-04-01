#include "fs/archive_self_ncch.hpp"
#include <memory>

// The part of the NCCH archive we're trying to access. Depends on the first 4 bytes of the binary file path
namespace PathType {
	enum : u32 {
		RomFS = 0,
		ExeFS = 2
	};
};

FSResult SelfNCCHArchive::createFile(const FSPath& path, u64 size) {
	Helpers::panic("[SelfNCCH] CreateFile not yet supported");
	return FSResult::Success;
}

FSResult SelfNCCHArchive::deleteFile(const FSPath& path) {
	Helpers::panic("[SelfNCCH] Unimplemented DeleteFile");
	return FSResult::Success;
}

FileDescriptor SelfNCCHArchive::openFile(const FSPath& path, const FilePerms& perms) {
	if (!hasRomFS()) {
		printf("Tried to open a SelfNCCH file without a RomFS\n");
		return FileError;
	}

	if (path.type != PathType::Binary || path.binary.size() != 12) {
		printf("Invalid SelfNCCH path type\n");
		return FileError;
	}

	// Where to read the file from. (https://www.3dbrew.org/wiki/Filesystem_services#SelfNCCH_File_Path_Data_Format)
	// We currently only know how to read from an NCCH's RomFS, ie type = 0
	const u32 type = *(u32*)&path.binary[0]; // TODO: Get rid of UB here
	if (type != PathType::RomFS && type != PathType::ExeFS) {
		Helpers::panic("Read from NCCH's non-RomFS & non-exeFS section!");
	}

	return NoFile; // No file descriptor needed for RomFS
}

ArchiveBase* SelfNCCHArchive::openArchive(const FSPath& path) {
	if (path.type != PathType::Empty) {
		printf("Invalid path type for SelfNCCH archive: %d\n", path.type);
		return nullptr;
	}

	return this;
}

std::optional<u32> SelfNCCHArchive::readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) {
	const FSPath& path = file->path; // Path of the file
	const u32 type = *(u32*)&path.binary[0]; // Type of the path

	if (type == PathType::RomFS && !hasRomFS()) {
		Helpers::panic("Tried to read file from non-existent RomFS");
		return std::nullopt;
	}

	if (type == PathType::ExeFS && !hasExeFS()) {
		Helpers::panic("Tried to read file from non-existent RomFS");
		return std::nullopt;
	}

	if (!file->isOpen) {
		printf("Tried to read from closed SelfNCCH file session");
		return std::nullopt;
	}

	auto cxi = mem.getCXI();
	IOFile& ioFile = mem.CXIFile;

	// Seek to file offset depending on if we're reading from RomFS, ExeFS, etc
	switch (type) {
		case PathType::RomFS: {
			const u32 romFSSize = cxi->romFS.size;
			const u32 romFSOffset = cxi->romFS.offset;
			if ((offset >> 32) || (offset >= romFSSize) || (offset + size >= romFSSize)) {
				Helpers::panic("Tried to read from SelfNCCH with too big of an offset");
			}

			if (!ioFile.seek(cxi->fileOffset + romFSOffset + offset + 0x1000)) {
				Helpers::panic("Failed to seek while reading from RomFS");
			}
			break;
		}

		case PathType::ExeFS: {
			const u32 exeFSSize = cxi->exeFS.size;
			const u32 exeFSOffset = cxi->exeFS.offset;
			if ((offset >> 32) || (offset >= exeFSSize) || (offset + size >= exeFSSize)) {
				Helpers::panic("Tried to read from SelfNCCH with too big of an offset");
			}

			if (!ioFile.seek(cxi->fileOffset + exeFSOffset + offset)) { // TODO: Not sure if this needs the + 0x1000
				Helpers::panic("Failed to seek while reading from ExeFS");
			}
			break;
		}

		default:
			Helpers::panic("Unimplemented file path type for SelfNCCH archive");
	}

	std::unique_ptr<u8[]> data(new u8[size]);
	auto [success, bytesRead] = ioFile.readBytes(&data[0], size);

	if (!success) {
		Helpers::panic("Failed to read from SelfNCCH archive");
	}

	for (u64 i = 0; i < bytesRead; i++) {
		mem.write8(dataPointer + i, data[i]);
	}

	return bytesRead;
}