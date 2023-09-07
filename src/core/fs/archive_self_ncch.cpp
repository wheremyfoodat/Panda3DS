#include "fs/archive_self_ncch.hpp"
#include <memory>

// The part of the NCCH archive we're trying to access. Depends on the first 4 bytes of the binary file path
namespace PathType {
	enum : u32 {
		RomFS = 0,
		ExeFS = 2,
		UpdateRomFS = 5,
	};
};

HorizonResult SelfNCCHArchive::createFile(const FSPath& path, u64 size) {
	Helpers::panic("[SelfNCCH] CreateFile not yet supported");
	return Result::Success;
}

HorizonResult SelfNCCHArchive::deleteFile(const FSPath& path) {
	Helpers::panic("[SelfNCCH] Unimplemented DeleteFile");
	return Result::Success;
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
	if (type != PathType::RomFS && type != PathType::ExeFS && type != PathType::UpdateRomFS) {
		Helpers::panic("Read from NCCH's non-RomFS & non-exeFS section! Path type: %d", type);
	}

	return NoFile; // No file descriptor needed for RomFS
}

Rust::Result<ArchiveBase*, HorizonResult> SelfNCCHArchive::openArchive(const FSPath& path) {
	if (path.type != PathType::Empty) {
		Helpers::panic("Invalid path type for SelfNCCH archive: %d\n", path.type);
		return Err(Result::FS::NotFoundInvalid);
	}

	return Ok((ArchiveBase*)this);
}

std::optional<u32> SelfNCCHArchive::readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) {
	const FSPath& path = file->path;          // Path of the file
	const u32 type = *(u32*)&path.binary[0];  // Type of the path

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

	bool success = false;
	std::size_t bytesRead = 0;
	std::unique_ptr<u8[]> data(new u8[size]);

	if (auto cxi = mem.getCXI(); cxi != nullptr) {
		IOFile& ioFile = mem.CXIFile;

		NCCH::FSInfo fsInfo;

		// Seek to file offset depending on if we're reading from RomFS, ExeFS, etc
		switch (type) {
			case PathType::RomFS: {
				const u64 romFSSize = cxi->romFS.size;
				const u64 romFSOffset = cxi->romFS.offset;
				if ((offset >> 32) || (offset >= romFSSize) || (offset + size >= romFSSize)) {
					Helpers::panic("Tried to read from SelfNCCH with too big of an offset");
				}

				fsInfo = cxi->romFS;
				offset += 0x1000;
				break;
			}

			case PathType::ExeFS: {
				const u64 exeFSSize = cxi->exeFS.size;
				const u64 exeFSOffset = cxi->exeFS.offset;
				if ((offset >> 32) || (offset >= exeFSSize) || (offset + size >= exeFSSize)) {
					Helpers::panic("Tried to read from SelfNCCH with too big of an offset");
				}

				fsInfo = cxi->exeFS;
				break;
			}

			// Normally, the update RomFS should overlay the cartridge RomFS when reading from this and an update is installed.
			// So to support updates, we need to perform this overlaying. For now, read from the cartridge RomFS.
			case PathType::UpdateRomFS: {
				Helpers::warn("Reading from update RomFS but updates are currently not supported! Reading from regular RomFS instead\n");

				const u64 romFSSize = cxi->romFS.size;
				const u64 romFSOffset = cxi->romFS.offset;
				if ((offset >> 32) || (offset >= romFSSize) || (offset + size >= romFSSize)) {
					Helpers::panic("Tried to read from SelfNCCH with too big of an offset");
				}

				fsInfo = cxi->romFS;
				offset += 0x1000;
				break;
			}

			default: Helpers::panic("Unimplemented file path type for SelfNCCH archive");
		}

		std::tie(success, bytesRead) = cxi->readFromFile(ioFile, fsInfo, &data[0], offset, size);
	}

	else if (auto hb3dsx = mem.get3DSX(); hb3dsx != nullptr) {
		switch (type) {
			case PathType::RomFS: {
				const u64 romFSSize = hb3dsx->romFSSize;
				if ((offset >> 32) || (offset >= romFSSize) || (offset + size >= romFSSize)) {
					Helpers::panic("Tried to read from SelfNCCH with too big of an offset");
				}
				break;
			}

			default: Helpers::panic("Unimplemented file path type for 3DSX SelfNCCH archive");
		}

		std::tie(success, bytesRead) = hb3dsx->readRomFSBytes(&data[0], offset, size);
	}

	if (!success) {
		Helpers::panic("Failed to read from SelfNCCH archive");
	}

	for (u64 i = 0; i < bytesRead; i++) {
		mem.write8(u32(dataPointer + i), data[i]);
	}

	return u32(bytesRead);
}