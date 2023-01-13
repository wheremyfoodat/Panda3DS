#include "fs/archive_ext_save_data.hpp"
#include <memory>

bool ExtSaveDataArchive::openFile(const FSPath& path) {
	if (path.type != PathType::Binary) {
		Helpers::panic("ExtSaveData accessed with a non-binary path in OpenFile. Type: %d", path.type);
	}

	Helpers::panic("ExtSaveDataArchive::OpenFile: Failed");
	return false;
}

ArchiveBase* ExtSaveDataArchive::openArchive(const FSPath& path) {
	if (path.type != PathType::Binary || path.binary.size() != 12) {
		Helpers::panic("ExtSaveData accessed with an invalid path in OpenArchive");
	}

	u32 mediaType = *(u32*)&path.binary[0];
	u64 saveID = *(u64*)&path.binary[4]; // TODO: Get rid of UB here.

	Helpers::panic("ExtSaveData: media type = %d\n", mediaType);

	return this;
}

std::optional<u32> ExtSaveDataArchive::readFile(FileSession* file, u64 offset, u32 size, u32 dataPointer) {
	Helpers::panic("ExtSaveDataArchive::ReadFile: Failed");
	return std::nullopt;
}