#include <algorithm>
#include "fs/archive_system_save_data.hpp"

namespace fs = std::filesystem;

Rust::Result<ArchiveBase*, HorizonResult> SystemSaveDataArchive::openArchive(const FSPath& path) {
	if (path.type != PathType::Empty) {
		Helpers::panic("Unimplemented path type for SystemSaveData::OpenArchive");
	}

	return Ok((ArchiveBase*)this);
}