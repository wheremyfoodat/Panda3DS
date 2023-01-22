#include "fs/archive_ncch.hpp"
#include <memory>

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
	if (type != 0) {
		Helpers::panic("Read from NCCH's non-RomFS section!");
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
	if (!hasRomFS()) {
		Helpers::panic("Tried to read file from non-existent RomFS");
		return std::nullopt;
	}

	if (!file->isOpen) {
		printf("Tried to read from closed SelfNCCH file session");
		return std::nullopt;
	}

	auto cxi = mem.getCXI();
	const u32 romFSSize = cxi->romFS.size;
	const u32 romFSOffset = cxi->romFS.offset;
	if ((offset >> 32) || (offset >= romFSSize) || (offset + size >= romFSSize)) {
		Helpers::panic("Tried to read from SelfNCCH with too big of an offset");
	}

	IOFile& ioFile = mem.CXIFile;
	if (!ioFile.seek(cxi->fileOffset + romFSOffset + offset + 0x1000)) {
		Helpers::panic("Failed to seek while reading from RomFS");
	}

	std::unique_ptr<u8[]> data(new u8[size]);
	auto [success, bytesRead] = ioFile.readBytes(&data[0], size);

	if (!success) {
		Helpers::panic("Failed to read from RomFS");
	}

	for (u64 i = 0; i < bytesRead; i++) {
		mem.write8(dataPointer + i, data[i]);
	}

	return bytesRead;
}