#include "fs/archive_ncch.hpp"
#include <memory>

bool SelfNCCHArchive::openFile(const FSPath& path) {
	if (!hasRomFS()) {
		printf("Tried to open a SelfNCCH file without a RomFS\n");
		return false;
	}

	if (path.type != PathType::Binary) {
		printf("Invalid SelfNCCH path type\n");
		return false;
	}

	// We currently only know how to read from an NCCH's RomFS
	if (mem.read32(path.pointer) != 0) {
		Helpers::panic("Read from NCCH's non-RomFS section!");
	}

	return true;
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