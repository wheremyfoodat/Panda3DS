#include "fs/archive_ncch.hpp"
#include "fs/bad_word_list.hpp"
#include "fs/country_list.hpp"
#include "fs/mii_data.hpp"
#include <algorithm>
#include <memory>

namespace PathType {
	enum : u32 {
		RomFS = 0,
		Code = 1,
		ExeFS = 2,
	};
};

namespace MediaType {
	enum : u8 {
		NAND = 0,
		SD = 1,
		Gamecard = 2
	};
};

HorizonResult NCCHArchive::createFile(const FSPath& path, u64 size) {
	Helpers::panic("[NCCH] CreateFile not yet supported");
	return Result::Success;
}

HorizonResult NCCHArchive::deleteFile(const FSPath& path) {
	Helpers::panic("[NCCH] Unimplemented DeleteFile");
	return Result::Success;
}

FileDescriptor NCCHArchive::openFile(const FSPath& path, const FilePerms& perms) {
	if (path.type != PathType::Binary || path.binary.size() != 20) {
		Helpers::panic("NCCHArchive::OpenFile: Invalid path");
	}

	const u32 media = *(u32*)&path.binary[0]; // 0 for NCCH, 1 for SaveData
	if (media != 0)
		Helpers::panic("NCCHArchive::OpenFile: Tried to read non-NCCH file");

	// Third word of the binary path indicates what we're reading from.
	const u32 type = *(u32*)&path.binary[8];
	if (media == 0 && type > 2)
		Helpers::panic("NCCHArchive::OpenFile: Invalid file path type");

	return NoFile;
}

Rust::Result<ArchiveBase*, HorizonResult> NCCHArchive::openArchive(const FSPath& path) {
	if (path.type != PathType::Binary || path.binary.size() != 16) {
		Helpers::panic("NCCHArchive::OpenArchive: Invalid path");
	}

	const u32 mediaType = path.binary[8];
	if (mediaType != 0)
		Helpers::panic("NCCH archive. Tried to access a mediatype other than the NAND. Type: %d", mediaType);

	return Ok((ArchiveBase*)this);
}

std::optional<u32> NCCHArchive::readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) {
	const auto& path = file->path.binary; // Path of the file
	const auto& archivePath = file->archivePath.binary; // Path of the archive

	const auto mediaType = archivePath[8];

	const auto media = *(u32*)&path[0]; // 0 for NCCH, 1 for savedata
	const auto partition = *(u32*)&path[4];
	const auto type = *(u32*)&path[8]; // Type of the path

	if (mediaType == MediaType::NAND) {
		const u32 lowProgramID = *(u32*)&archivePath[0];
		const u32 highProgramID = *(u32*)&archivePath[4];

		// High Title ID of the archive (from Citra). https://3dbrew.org/wiki/Title_list.
		constexpr u32 sharedDataArchive = 0x0004009B;
		constexpr u32 systemDataArchive = 0x000400DB;

		// Low ID (taken from Citra)
		constexpr u32 miiData = 0x00010202;
		constexpr u32 regionManifest = 0x00010402;
		constexpr u32 badWordList = 0x00010302;
		constexpr u32 sharedFont = 0x00014002;
		std::vector<u8> fileData;

		if (highProgramID == sharedDataArchive) {
			if (lowProgramID == miiData) fileData = std::vector<u8>(std::begin(MII_DATA), std::end(MII_DATA));
			else if (lowProgramID == regionManifest) fileData = std::vector<u8>(std::begin(COUNTRY_LIST_DATA), std::end(COUNTRY_LIST_DATA));
			else Helpers::panic("[NCCH archive] Read unimplemented NAND file. ID: %08X", lowProgramID);
		} else if (highProgramID == systemDataArchive && lowProgramID == badWordList) {
			fileData = std::vector<u8>(std::begin(BAD_WORD_LIST_DATA), std::end(BAD_WORD_LIST_DATA));
		} else {
			Helpers::panic("[NCCH archive] Read from unimplemented NCCH archive file. High program ID: %08X, low ID: %08X",
				highProgramID, lowProgramID);
		}

		if (offset >= fileData.size()) {
			Helpers::panic("[NCCH archive] Out of bounds read from NAND file");
		}

		u32 availableBytes = u32(fileData.size() - offset); // How many bytes we can read from the file
		u32 bytesRead = std::min<u32>(size, availableBytes); // Cap the amount of bytes to read if we're going to go out of bounds
		for (u32 i = 0; i < bytesRead; i++) {
			mem.write8(dataPointer + i, fileData[offset + i]);
		}

		return bytesRead;
	} else {
		Helpers::panic("NCCH archive tried to read non-NAND file");
	}

	// Code below is for mediaType == 2 (gamecard). Currently unused
	if (partition != 0)
		Helpers::panic("[NCCH] Tried to read from non-zero partition");

	if (type == PathType::RomFS && !hasRomFS()) {
		Helpers::panic("Tried to read file from non-existent RomFS");
		return std::nullopt;
	}

	if (type == PathType::ExeFS && !hasExeFS()) {
		Helpers::panic("Tried to read file from non-existent RomFS");
		return std::nullopt;
	}

	if (!file->isOpen) {
		printf("Tried to read from closed NCCH file session");
		return std::nullopt;
	}

	auto cxi = mem.getCXI();

	// Seek to file offset depending on if we're reading from RomFS, ExeFS, etc
	switch (type) {
		case PathType::RomFS: {
			const u64 romFSSize = cxi->romFS.size;
			const u64 romFSOffset = cxi->romFS.offset;
			if ((offset >> 32) || (offset >= romFSSize) || (offset + size >= romFSSize)) {
				Helpers::panic("Tried to read from NCCH with too big of an offset");
			}

			offset += 0x1000;
			break;
		}

		default:
			Helpers::panic("Unimplemented file path type for NCCH archive");
	}

	std::unique_ptr<u8[]> data(new u8[size]);
	auto [success, bytesRead] = cxi->readFromFile(mem.CXIFile, cxi->romFS, &data[0], offset, size);

	if (!success) {
		Helpers::panic("Failed to read from NCCH archive");
	}

	for (u64 i = 0; i < bytesRead; i++) {
		mem.write8(u32(dataPointer + i), data[i]);
	}

	return u32(bytesRead);
}