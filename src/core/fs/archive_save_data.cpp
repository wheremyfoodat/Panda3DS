#include "fs/archive_save_data.hpp"
#include <algorithm>
#include <memory>

CreateFileResult SaveDataArchive::createFile(const FSPath& path, u64 size) {
	Helpers::panic("[SaveData] CreateFile not yet supported");
	return CreateFileResult::Success;
}

FileDescriptor SaveDataArchive::openFile(const FSPath& path, const FilePerms& perms) {
	if (!cartHasSaveData()) {
		printf("Tried to read SaveData FS without save data\n");
		return FileError;
	}

	if (path.type == PathType::UTF16 /* && path.utf16_string == u"/game_header" */) {
		printf("Opened file from the SaveData archive \n");
		return NoFile;
	}

	if (path.type != PathType::Binary) {
		printf("Unimplemented SaveData path type: %d\n", path.type);
		return FileError;
	}

	return NoFile;
}

ArchiveBase* SaveDataArchive::openArchive(const FSPath& path) {
	if (path.type != PathType::Empty) {
		Helpers::panic("Unimplemented path type for SaveData archive: %d\n", path.type);
		return nullptr;
	}

	return this;
}

std::optional<u32> SaveDataArchive::readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) {
	if (!cartHasSaveData()) {
		printf("Tried to read SaveData FS without save data\n");
		return std::nullopt;
	}

	auto cxi = mem.getCXI();
	const u64 saveSize = cxi->saveData.size();

	if (offset >= saveSize) {
		printf("Tried to read from past the end of save data\n");
		return std::nullopt;
	}

	const u64 endOffset = std::min<u64>(saveSize, offset + size); // Don't go past the end of the save file
	const u32 bytesRead = endOffset - offset;

	if (bytesRead != 0x22) Helpers::panic("Might want to actually implement SaveData");

	static constexpr std::array<u8, 0x22> saveDataStub = { 0x00, 0x23, 0x3C, 0x77, 0x67, 0x28, 0x30, 0x33, 0x58, 0x61, 0x39, 0x61, 0x48, 0x59, 0x36, 0x55, 0x43, 0x76, 0x58, 0x61, 0x6F, 0x65, 0x48, 0x6D, 0x2B, 0x5E, 0x6F, 0x62, 0x3E, 0x6F, 0x34, 0x00, 0x77, 0x09};

	for (u32 i = 0; i < bytesRead; i++) {
		mem.write8(dataPointer + i, saveDataStub[i]);
	}

	return bytesRead;
}