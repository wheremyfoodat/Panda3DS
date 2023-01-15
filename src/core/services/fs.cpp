#include "services/fs.hpp"
#include "kernel/kernel.hpp"

namespace FSCommands {
	enum : u32 {
		Initialize = 0x08010002,
		OpenFile = 0x080201C2,
		OpenFileDirectly = 0x08030204,
		OpenArchive = 0x080C00C2,
		CloseArchive = 0x080E0080,
		IsSdmcDetected = 0x08170000,
		InitializeWithSdkVersion = 0x08610042,
		SetPriority = 0x08620040,
		GetPriority = 0x08630000
	};
}

namespace Result {
	enum : u32 {
		Success = 0,
		FileNotFound = 0xC8804464, // TODO: Verify this
		Failure = 0xFFFFFFFF,
	};
}

void FSService::reset() {
	priority = 0;
}

ArchiveBase* FSService::getArchiveFromID(u32 id) {
	switch (id) {
		case ArchiveID::SelfNCCH: return &selfNcch;
		case ArchiveID::SaveData: return &saveData;
		case ArchiveID::SharedExtSaveData: return &sharedExtSaveData;
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

FSPath FSService::readPath(u32 type, u32 pointer, u32 size) {
	std::vector<u8> data;
	data.resize(size);

	for (u32 i = 0; i < size; i++)
		data[i] = mem.read8(pointer + i);

	return FSPath(type, data);
}

void FSService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case FSCommands::CloseArchive: closeArchive(messagePointer); break;
		case FSCommands::GetPriority: getPriority(messagePointer); break;
		case FSCommands::Initialize: initialize(messagePointer); break;
		case FSCommands::InitializeWithSdkVersion: initializeWithSdkVersion(messagePointer); break;
		case FSCommands::IsSdmcDetected: isSdmcDetected(messagePointer); break;
		case FSCommands::OpenArchive: openArchive(messagePointer); break;
		case FSCommands::OpenFile: openFile(messagePointer); break;
		case FSCommands::OpenFileDirectly: openFileDirectly(messagePointer); break;
		case FSCommands::SetPriority: setPriority(messagePointer); break;
		default: Helpers::panic("FS service requested. Command: %08X\n", command);
	}
}

void FSService::initialize(u32 messagePointer) {
	log("FS::Initialize\n");
	mem.write32(messagePointer + 4, Result::Success);
}

// TODO: Figure out how this is different from Initialize
void FSService::initializeWithSdkVersion(u32 messagePointer) {
	const auto version = mem.read32(messagePointer + 4);
	log("FS::InitializeWithSDKVersion(version = %d)\n", version);

	initialize(messagePointer);
}

void FSService::closeArchive(u32 messagePointer) {
	const Handle handle = static_cast<u32>(mem.read64(messagePointer + 4)); // TODO: archive handles should be 64-bit
	const auto object = kernel.getObject(handle, KernelObjectType::Archive);
	log("FSService::CloseArchive(handle = %X)\n", handle);

	if (object == nullptr) {
		log("FSService::CloseArchive: Tried to close invalid archive %X\n", handle);
		mem.write32(messagePointer + 4, Result::Failure);
	} else {
		object->getData<ArchiveSession>()->isOpen = false;
		mem.write32(messagePointer + 4, Result::Success);
	}
}

void FSService::openArchive(u32 messagePointer) {
	const u32 archiveID = mem.read32(messagePointer + 4);
	const u32 archivePathType = mem.read32(messagePointer + 8);
	const u32 archivePathSize = mem.read32(messagePointer + 12);
	const u32 archivePathPointer = mem.read32(messagePointer + 20);

	auto archivePath = readPath(archivePathType, archivePathPointer, archivePathSize);
	log("FS::OpenArchive(archive ID = %d, archive path type = %d)\n", archiveID, archivePathType);
	
	std::optional<Handle> handle = openArchiveHandle(archiveID, archivePath);
	if (handle.has_value()) {
		mem.write32(messagePointer + 4, Result::Success);
		mem.write64(messagePointer + 8, handle.value());
	} else {
		log("FS::OpenArchive: Failed to open archive with id = %d\n", archiveID);
		mem.write32(messagePointer + 4, Result::Failure);
	}
}

void FSService::openFile(u32 messagePointer) {
	const u32 archiveHandle = mem.read64(messagePointer + 8);
	const u32 filePathType = mem.read32(messagePointer + 16);
	const u32 filePathSize = mem.read32(messagePointer + 20);
	const u32 openFlags = mem.read32(messagePointer + 24);
	const u32 attributes = mem.read32(messagePointer + 28);
	const u32 filePathPointer = mem.read32(messagePointer + 36);

	log("FS::OpenFile\n");

	auto archiveObject = kernel.getObject(archiveHandle, KernelObjectType::Archive);
	if (archiveObject == nullptr) [[unlikely]] {
		log("FS::OpenFile: Invalid archive handle %d\n", archiveHandle);
		mem.write32(messagePointer + 4, Result::Failure);
		return;
	}

	ArchiveBase* archive = archiveObject->getData<ArchiveSession>()->archive;
	auto filePath = readPath(filePathType, filePathPointer, filePathSize);

	std::optional<Handle> handle = openFileHandle(archive, filePath);
	if (!handle.has_value()) {
		printf("OpenFile failed\n");
		mem.write32(messagePointer + 4, Result::FileNotFound);
	} else {
		mem.write32(messagePointer + 4, Result::Success);
		mem.write32(messagePointer + 8, 0x10); // "Move handle descriptor"
		mem.write32(messagePointer + 12, handle.value());
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

	auto archivePath = readPath(archivePathType, archivePathPointer, archivePathSize);
	auto filePath = readPath(filePathType, filePathPointer, filePathSize);

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

void FSService::getPriority(u32 messagePointer) {
	log("FS::GetPriority\n");

	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, priority);
}

void FSService::setPriority(u32 messagePointer) {
	const u32 value = mem.read32(messagePointer + 4);
	log("FS::SetPriority (priority = %d)\n", value);
	
	mem.write32(messagePointer + 4, Result::Success);
	priority = value;
}

void FSService::isSdmcDetected(u32 messagePointer) {
	log("FS::IsSdmcDetected\n");
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0); // Whether SD is detected. For now we emulate a 3DS without an SD.
}