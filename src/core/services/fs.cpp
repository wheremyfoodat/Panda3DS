#include "services/fs.hpp"
#include "kernel/kernel.hpp"

namespace FSCommands {
	enum : u32 {
		Initialize = 0x08010002,
		OpenFileDirectly = 0x08030204,
		OpenArchive = 0x080C00C2
	};
}

namespace Result {
	enum : u32 {
		Success = 0,
		Failure = 0xFFFFFFFF
	};
}

void FSService::reset() {}

ArchiveBase* FSService::getArchiveFromID(u32 id) {
	switch (id) {
		case ArchiveID::SelfNCCH: return &selfNcch;
		case ArchiveID::SaveData: return &saveData;
		case ArchiveID::SDMC: return &sdmc;
		default:
			Helpers::panic("Unknown archive. ID: %d\n", id);
			return nullptr;
	}
}

std::optional<Handle> FSService::openFileHandle(ArchiveBase* archive, const FSPath& path) {
	bool opened = archive->openFile(path);
	if (opened) {
		auto handle = kernel.makeObject(KernelObjectType::File);
		auto& file = kernel.getObjects()[handle];
		file.data = new FileSession(archive, path);
		
		return handle;
	} else {
		return std::nullopt;
	}
}

std::optional<Handle> FSService::openArchiveHandle(u32 archiveID, const FSPath& path) {
	ArchiveBase* archive = getArchiveFromID(archiveID);

	if (archive == nullptr) [[unlikely]] {
		Helpers::panic("OpenArchive: Tried to open unknown archive %d.", archiveID);
		return std::nullopt;
	}

	bool opened = archive->openArchive(path);
	if (opened) {
		auto handle = kernel.makeObject(KernelObjectType::Archive);
		auto& archiveObject = kernel.getObjects()[handle];
		archiveObject.data = new ArchiveSession(archive, path);

		return handle;
	}
	else {
		return std::nullopt;
	}
}

void FSService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case FSCommands::Initialize: initialize(messagePointer); break;
		case FSCommands::OpenArchive: openArchive(messagePointer); break;
		case FSCommands::OpenFileDirectly: openFileDirectly(messagePointer); break;
		default: Helpers::panic("FS service requested. Command: %08X\n", command);
	}
}

void FSService::initialize(u32 messagePointer) {
	log("FS::Initialize\n");
	mem.write32(messagePointer + 4, Result::Success);
}

void FSService::openArchive(u32 messagePointer) {
	const u32 archiveID = mem.read32(messagePointer + 4);
	const u32 archivePathType = mem.read32(messagePointer + 8);
	const u32 archivePathSize = mem.read32(messagePointer + 12);
	const u32 archivePathPointer = mem.read32(messagePointer + 20);
	FSPath archivePath{ .type = archivePathType, .size = archivePathSize, .pointer = archivePathPointer };

	log("FS::OpenArchive (archive ID = %d, archive path type = %d)\n", archiveID, archivePathType);
	
	std::optional<Handle> handle = openArchiveHandle(archiveID, archivePath);
	if (handle.has_value()) {
		mem.write32(messagePointer + 4, Result::Success);
		mem.write64(messagePointer + 8, handle.value());
	} else {
		log("FS::OpenArchive: Failed to open archive with id = %d\n", archiveID);
		mem.write32(messagePointer + 4, Result::Failure);
	}
}

void FSService::openFileDirectly(u32 messagePointer) {
	const u32 archiveID = mem.read32(messagePointer + 8);
	const u32 archivePathType = mem.read32(messagePointer + 12);
	const u32 archivePathSize = mem.read32(messagePointer + 16);
	const u32 filePathType = mem.read32(messagePointer + 20);
	const u32 filePathSize = mem.read32(messagePointer + 24);
	const u32 openFlags = mem.read32(messagePointer + 28);
	const u32 attributes = mem.read32(messagePointer + 32);
	const u32 archivePathPointer = mem.read32(messagePointer + 40);
	const u32 filePathPointer = mem.read32(messagePointer + 48);

	log("FS::OpenFileDirectly\n");

	ArchiveBase* archive = getArchiveFromID(archiveID);
	if (archive == nullptr) [[unlikely]] {
		Helpers::panic("OpenFileDirectly: Tried to open unknown archive %d.", archiveID);
	}

	FSPath archivePath { .type = archivePathType, .size = archivePathSize, .pointer = archivePathPointer };
	FSPath filePath { .type = filePathType, .size = filePathSize, .pointer = filePathPointer };

	archive = archive->openArchive(archivePath);
	if (archive == nullptr) [[unlikely]] {
		Helpers::panic("OpenFileDirectly: Failed to open archive with given path");
	}

	std::optional<Handle> handle = openFileHandle(archive, filePath);
	if (!handle.has_value()) {
		Helpers::panic("OpenFileDirectly: Failed to open file with given path");
	} else {
		mem.write32(messagePointer + 4, Result::Success);
		mem.write32(messagePointer + 12, handle.value());
	}
}