#include "services/fs.hpp"
#include "kernel/kernel.hpp"
#include "io_file.hpp"

#ifdef CreateFile // windows.h defines CreateFile & DeleteFile because of course it does.
#undef CreateFile
#undef DeleteFile
#endif

namespace FSCommands {
	enum : u32 {
		Initialize = 0x08010002,
		OpenFile = 0x080201C2,
		OpenFileDirectly = 0x08030204,
		DeleteFile = 0x08040142,
		CreateFile = 0x08080202,
		OpenDirectory = 0x080B0102,
		OpenArchive = 0x080C00C2,
		CloseArchive = 0x080E0080,
		IsSdmcDetected = 0x08170000,
		GetFormatInfo = 0x084500C2,
		FormatSaveData = 0x084C0242,
		InitializeWithSdkVersion = 0x08610042,
		SetPriority = 0x08620040,
		GetPriority = 0x08630000
	};
}

namespace ResultCode {
	enum : u32 {
		Success = 0,
		FileNotFound = 0xC8804464, // TODO: Verify this
		Failure = 0xFFFFFFFF,
	};
}

void FSService::reset() {
	priority = 0;
}

// Creates directories for NAND, ExtSaveData, etc if they don't already exist. Should be executed after loading a new ROM.
void FSService::initializeFilesystem() {
	const auto nandPath = IOFile::getAppData() / "NAND"; // Create NAND
	const auto cartPath = IOFile::getAppData() / "CartSave"; // Create cartridge save folder for use with ExtSaveData
	const auto savePath = IOFile::getAppData() / "SaveData"; // Create SaveData
	namespace fs = std::filesystem;
	// TODO: SDMC, etc

	if (!fs::is_directory(nandPath)) {
		fs::create_directories(nandPath);
	}

	if (!fs::is_directory(cartPath)) {
		fs::create_directories(cartPath);
	}

	if (!fs::is_directory(savePath)) {
		fs::create_directories(savePath);
	}
}

ArchiveBase* FSService::getArchiveFromID(u32 id, const FSPath& archivePath) {
	switch (id) {
		case ArchiveID::SelfNCCH: return &selfNcch;
		case ArchiveID::SaveData: return &saveData;
		case ArchiveID::ExtSaveData:
			if (archivePath.type == PathType::Binary) {
				switch (archivePath.binary[0]) {
					case 0: return &extSaveData_nand;
					case 1: return &extSaveData_cart;
				}
			}
			return nullptr;

		case ArchiveID::SharedExtSaveData:
			if (archivePath.type == PathType::Binary) {
				switch (archivePath.binary[0]) {
					case 0: return &sharedExtSaveData_nand;
					case 1: return &sharedExtSaveData_cart;
				}
			}
			return nullptr;

		case ArchiveID::SDMC: return &sdmc;
		case ArchiveID::SavedataAndNcch: return &ncch; // This can only access NCCH outside of FSPXI
		default:
			Helpers::panic("Unknown archive. ID: %d\n", id);
			return nullptr;
	}
}

std::optional<Handle> FSService::openFileHandle(ArchiveBase* archive, const FSPath& path, const FSPath& archivePath, const FilePerms& perms) {
	FileDescriptor opened = archive->openFile(path, perms);
	if (opened.has_value()) { // If opened doesn't have a value, we failed to open the file
		auto handle = kernel.makeObject(KernelObjectType::File);

		auto& file = kernel.getObjects()[handle];
		file.data = new FileSession(archive, path, archivePath, opened.value());
		
		return handle;
	} else {
		return std::nullopt;
	}
}

Rust::Result<Handle, FSResult> FSService::openDirectoryHandle(ArchiveBase* archive, const FSPath& path) {
	Rust::Result<DirectorySession, FSResult> opened = archive->openDirectory(path);
	if (opened.isOk()) { // If opened doesn't have a value, we failed to open the directory
		auto handle = kernel.makeObject(KernelObjectType::Directory);
		auto& object = kernel.getObjects()[handle];
		object.data = new DirectorySession(opened.unwrap());

		return Ok(handle);
	} else {
		return Err(opened.unwrapErr());
	}
}

std::optional<Handle> FSService::openArchiveHandle(u32 archiveID, const FSPath& path) {
	ArchiveBase* archive = getArchiveFromID(archiveID, path);

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
		case FSCommands::CreateFile: createFile(messagePointer); break;
		case FSCommands::CloseArchive: closeArchive(messagePointer); break;
		case FSCommands::DeleteFile: deleteFile(messagePointer); break;
		case FSCommands::FormatSaveData: formatSaveData(messagePointer); break;
		case FSCommands::GetFormatInfo: getFormatInfo(messagePointer); break;
		case FSCommands::GetPriority: getPriority(messagePointer); break;
		case FSCommands::Initialize: initialize(messagePointer); break;
		case FSCommands::InitializeWithSdkVersion: initializeWithSdkVersion(messagePointer); break;
		case FSCommands::IsSdmcDetected: isSdmcDetected(messagePointer); break;
		case FSCommands::OpenArchive: openArchive(messagePointer); break;
		case FSCommands::OpenDirectory: openDirectory(messagePointer); break;
		case FSCommands::OpenFile: [[likely]] openFile(messagePointer); break;
		case FSCommands::OpenFileDirectly: [[likely]] openFileDirectly(messagePointer); break;
		case FSCommands::SetPriority: setPriority(messagePointer); break;
		default: Helpers::panic("FS service requested. Command: %08X\n", command);
	}
}

void FSService::initialize(u32 messagePointer) {
	log("FS::Initialize\n");
	mem.write32(messagePointer + 4, ResultCode::Success);
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
		mem.write32(messagePointer + 4, ResultCode::Failure);
	} else {
		object->getData<ArchiveSession>()->isOpen = false;
		mem.write32(messagePointer + 4, ResultCode::Success);
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
		mem.write32(messagePointer + 4, ResultCode::Success);
		mem.write64(messagePointer + 8, handle.value());
	} else {
		log("FS::OpenArchive: Failed to open archive with id = %d\n", archiveID);
		mem.write32(messagePointer + 4, ResultCode::Failure);
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
		mem.write32(messagePointer + 4, ResultCode::Failure);
		return;
	}

	ArchiveBase* archive = archiveObject->getData<ArchiveSession>()->archive;
	const FSPath& archivePath = archiveObject->getData<ArchiveSession>()->path;

	auto filePath = readPath(filePathType, filePathPointer, filePathSize);
	const FilePerms perms(openFlags);

	std::optional<Handle> handle = openFileHandle(archive, filePath, archivePath, perms);
	if (!handle.has_value()) {
		printf("OpenFile failed\n");
		mem.write32(messagePointer + 4, ResultCode::FileNotFound);
	} else {
		mem.write32(messagePointer + 4, ResultCode::Success);
		mem.write32(messagePointer + 8, 0x10); // "Move handle descriptor"
		mem.write32(messagePointer + 12, handle.value());
	}
}

void FSService::openDirectory(u32 messagePointer) {
	log("FS::OpenDirectory\n");
	const Handle archiveHandle = (Handle)mem.read64(messagePointer + 4);
	const u32 pathType = mem.read32(messagePointer + 12);
	const u32 pathSize = mem.read32(messagePointer + 16);
	const u32 pathPointer = mem.read32(messagePointer + 24);

	KernelObject* archiveObject = kernel.getObject(archiveHandle, KernelObjectType::Archive);
	if (archiveObject == nullptr) [[unlikely]] {
		log("FS::OpenDirectory: Invalid archive handle %d\n", archiveHandle);
		mem.write32(messagePointer + 4, ResultCode::Failure);
		return;
	}

	ArchiveBase* archive = archiveObject->getData<ArchiveSession>()->archive;
	const auto dirPath = readPath(pathType, pathPointer, pathSize);
	auto dir = openDirectoryHandle(archive, dirPath);

	if (dir.isOk()) {
		mem.write32(messagePointer + 4, ResultCode::Success);
		mem.write32(messagePointer + 12, dir.unwrap());
	} else {
		printf("FS::OpenDirectory failed\n");
		mem.write32(messagePointer + 4, static_cast<u32>(dir.unwrapErr()));
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

	auto archivePath = readPath(archivePathType, archivePathPointer, archivePathSize);
	ArchiveBase* archive = getArchiveFromID(archiveID, archivePath);
	
	if (archive == nullptr) [[unlikely]] {
		Helpers::panic("OpenFileDirectly: Tried to open unknown archive %d.", archiveID);
	}
	auto filePath = readPath(filePathType, filePathPointer, filePathSize);
	const FilePerms perms(openFlags);

	archive = archive->openArchive(archivePath);
	if (archive == nullptr) [[unlikely]] {
		Helpers::panic("OpenFileDirectly: Failed to open archive with given path");
	}

	std::optional<Handle> handle = openFileHandle(archive, filePath, archivePath, perms);
	if (!handle.has_value()) {
		Helpers::panic("OpenFileDirectly: Failed to open file with given path");
	} else {
		mem.write32(messagePointer + 4, ResultCode::Success);
		mem.write32(messagePointer + 12, handle.value());
	}
}

void FSService::createFile(u32 messagePointer) {
	const Handle archiveHandle = mem.read64(messagePointer + 8);
	const u32 filePathType = mem.read32(messagePointer + 16);
	const u32 filePathSize = mem.read32(messagePointer + 20);
	const u32 attributes = mem.read32(messagePointer + 24);
	const u64 size = mem.read64(messagePointer + 28);
	const u32 filePathPointer = mem.read32(messagePointer + 40);

	log("FS::CreateFile\n");

	auto archiveObject = kernel.getObject(archiveHandle, KernelObjectType::Archive);
	if (archiveObject == nullptr) [[unlikely]] {
		log("FS::OpenFile: Invalid archive handle %d\n", archiveHandle);
		mem.write32(messagePointer + 4, ResultCode::Failure);
		return;
	}

	ArchiveBase* archive = archiveObject->getData<ArchiveSession>()->archive;
	auto filePath = readPath(filePathType, filePathPointer, filePathSize);

	FSResult res = archive->createFile(filePath, size);
	mem.write32(messagePointer + 4, static_cast<u32>(res));
}

void FSService::deleteFile(u32 messagePointer) {
	const u32 archiveHandle = mem.read64(messagePointer + 8);
	const u32 filePathType = mem.read32(messagePointer + 16);
	const u32 filePathSize = mem.read32(messagePointer + 20);
	const u32 filePathPointer = mem.read32(messagePointer + 28);

	log("FS::DeleteFile\n");
	auto archiveObject = kernel.getObject(archiveHandle, KernelObjectType::Archive);
	if (archiveObject == nullptr) [[unlikely]] {
		log("FS::DeleteFile: Invalid archive handle %d\n", archiveHandle);
		mem.write32(messagePointer + 4, ResultCode::Failure);
		return;
	}

	ArchiveBase* archive = archiveObject->getData<ArchiveSession>()->archive;
	auto filePath = readPath(filePathType, filePathPointer, filePathSize);

	FSResult res = archive->deleteFile(filePath);
	mem.write32(messagePointer + 4, static_cast<u32>(res));
}

void FSService::getFormatInfo(u32 messagePointer) {
	const u32 archiveID = mem.read32(messagePointer + 4);
	const u32 pathType = mem.read32(messagePointer + 8);
	const u32 pathSize = mem.read32(messagePointer + 12);
	const u32 pathPointer = mem.read32(messagePointer + 20);

	const auto path = readPath(pathType, pathPointer, pathSize);
	log("FS::GetFormatInfo(archive ID = %d, archive path type = %d)\n", archiveID, pathType);

	ArchiveBase* archive = getArchiveFromID(archiveID, path);
	if (archive == nullptr) [[unlikely]] {
		Helpers::panic("OpenArchive: Tried to open unknown archive %d.", archiveID);
	}

	ArchiveBase::FormatInfo info = archive->getFormatInfo(path);
	mem.write32(messagePointer + 4, ResultCode::Success);
	mem.write32(messagePointer + 8, info.size);
	mem.write32(messagePointer + 12, info.numOfDirectories);
	mem.write32(messagePointer + 16, info.numOfFiles);
	mem.write8(messagePointer + 20, info.duplicateData ? 1 : 0);
}

void FSService::formatSaveData(u32 messagePointer) {
	const u32 archiveID = mem.read32(messagePointer + 4);
	if (archiveID != ArchiveID::SaveData)
		Helpers::panic("FS::FormatSaveData: Archive is not SaveData");

	// Read path and path info
	const u32 pathType = mem.read32(messagePointer + 8);
	const u32 pathSize = mem.read32(messagePointer + 12);
	const u32 pathPointer = mem.read32(messagePointer + 44);
	auto path = readPath(pathType, pathPointer, pathSize);
	// Size of a block. Seems to always be 0x200
	const u32 blockSize = mem.read32(messagePointer + 16);

	if (blockSize != 0x200 && blockSize != 0x1000)
		Helpers::panic("FS::FormatSaveData: Invalid SaveData block size");

	const u32 directoryNum = mem.read32(messagePointer + 20); // Max number of directories
	const u32 fileNum = mem.read32(messagePointer + 24); // Max number of files
	const u32 directoryBucketNum = mem.read32(messagePointer + 28); // Not sure what a directory bucket is...?
	const u32 fileBucketNum = mem.read32(messagePointer + 32); // Same here
	const bool duplicateData = mem.read8(messagePointer + 36) != 0; 

	printf("Stubbed FS::FormatSaveData. File num: %d, directory num: %d\n", fileNum, directoryNum);
}

void FSService::getPriority(u32 messagePointer) {
	log("FS::GetPriority\n");

	mem.write32(messagePointer + 4, ResultCode::Success);
	mem.write32(messagePointer + 8, priority);
}

void FSService::setPriority(u32 messagePointer) {
	const u32 value = mem.read32(messagePointer + 4);
	log("FS::SetPriority (priority = %d)\n", value);
	
	mem.write32(messagePointer + 4, ResultCode::Success);
	priority = value;
}

void FSService::isSdmcDetected(u32 messagePointer) {
	log("FS::IsSdmcDetected\n");
	mem.write32(messagePointer + 4, ResultCode::Success);
	mem.write32(messagePointer + 8, 0); // Whether SD is detected. For now we emulate a 3DS without an SD.
}