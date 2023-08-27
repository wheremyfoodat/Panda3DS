#include "services/fs.hpp"
#include "kernel/kernel.hpp"
#include "io_file.hpp"
#include "ipc.hpp"

#ifdef CreateFile // windows.h defines CreateFile & DeleteFile because of course it does.
#undef CreateDirectory
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
		CreateDirectory = 0x08090182,
		OpenDirectory = 0x080B0102,
		OpenArchive = 0x080C00C2,
		ControlArchive = 0x080D0144,
		CloseArchive = 0x080E0080,
		FormatThisUserSaveData = 0x080F0180,
		GetFreeBytes = 0x08120080,
		IsSdmcDetected = 0x08170000,
		IsSdmcWritable = 0x08180000,
		GetFormatInfo = 0x084500C2,
		FormatSaveData = 0x084C0242,
		CreateExtSaveData = 0x08510242,
		DeleteExtSaveData = 0x08520100,
		InitializeWithSdkVersion = 0x08610042,
		SetPriority = 0x08620040,
		GetPriority = 0x08630000,
		GetThisSaveDataSecureValue = 0x086F0040,
		TheGameboyVCFunction = 0x08750180,
	};
}

void FSService::reset() {
	priority = 0;
}

// Creates directories for NAND, ExtSaveData, etc if they don't already exist. Should be executed after loading a new ROM.
void FSService::initializeFilesystem() {
	const auto sdmcPath = IOFile::getAppData() / "SDMC"; // Create SDMC directory
	const auto nandSharedpath = IOFile::getAppData() / ".." / "SharedFiles" / "NAND";

	const auto savePath = IOFile::getAppData() / "SaveData"; // Create SaveData
	const auto formatPath = IOFile::getAppData() / "FormatInfo"; // Create folder for storing archive formatting info
	namespace fs = std::filesystem;


	if (!fs::is_directory(nandSharedpath)) {
		fs::create_directories(nandSharedpath);
	}

	if (!fs::is_directory(sdmcPath)) {
		fs::create_directories(sdmcPath);
	}

	if (!fs::is_directory(savePath)) {
		fs::create_directories(savePath);
	}

	if (!fs::is_directory(formatPath)) {
		fs::create_directories(formatPath);
	}
}

ArchiveBase* FSService::getArchiveFromID(u32 id, const FSPath& archivePath) {
	switch (id) {
		case ArchiveID::SelfNCCH: return &selfNcch;
		case ArchiveID::SaveData: return &saveData;
		case ArchiveID::UserSaveData2: return &userSaveData2;

		case ArchiveID::ExtSaveData:
			return &extSaveData_sdmc;

		case ArchiveID::SharedExtSaveData:
			return &sharedExtSaveData_nand;

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

Rust::Result<Handle, Result::HorizonResult> FSService::openDirectoryHandle(ArchiveBase* archive, const FSPath& path) {
	Rust::Result<DirectorySession, Result::HorizonResult> opened = archive->openDirectory(path);
	if (opened.isOk()) { // If opened doesn't have a value, we failed to open the directory
		auto handle = kernel.makeObject(KernelObjectType::Directory);
		auto& object = kernel.getObjects()[handle];
		object.data = new DirectorySession(opened.unwrap());

		return Ok(handle);
	} else {
		return Err(opened.unwrapErr());
	}
}

Rust::Result<Handle, Result::HorizonResult> FSService::openArchiveHandle(u32 archiveID, const FSPath& path) {
	ArchiveBase* archive = getArchiveFromID(archiveID, path);

	if (archive == nullptr) [[unlikely]] {
		Helpers::panic("OpenArchive: Tried to open unknown archive %d.", archiveID);
		return Err(Result::FS::NotFormatted);
	}

	Rust::Result<ArchiveBase*, Result::HorizonResult> res = archive->openArchive(path);
	if (res.isOk()) {
		auto handle = kernel.makeObject(KernelObjectType::Archive);
		auto& archiveObject = kernel.getObjects()[handle];
		archiveObject.data = new ArchiveSession(res.unwrap(), path);

		return Ok(handle);
	}
	else {
		return Err(res.unwrapErr());
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
		case FSCommands::CreateDirectory: createDirectory(messagePointer); break;
		case FSCommands::CreateExtSaveData: createExtSaveData(messagePointer); break;
		case FSCommands::CreateFile: createFile(messagePointer); break;
		case FSCommands::ControlArchive: controlArchive(messagePointer); break;
		case FSCommands::CloseArchive: closeArchive(messagePointer); break;
		case FSCommands::DeleteExtSaveData: deleteExtSaveData(messagePointer); break;
		case FSCommands::DeleteFile: deleteFile(messagePointer); break;
		case FSCommands::FormatSaveData: formatSaveData(messagePointer); break;
		case FSCommands::FormatThisUserSaveData: formatThisUserSaveData(messagePointer); break;
		case FSCommands::GetFreeBytes: getFreeBytes(messagePointer); break;
		case FSCommands::GetFormatInfo: getFormatInfo(messagePointer); break;
		case FSCommands::GetPriority: getPriority(messagePointer); break;
		case FSCommands::GetThisSaveDataSecureValue: getThisSaveDataSecureValue(messagePointer); break;
		case FSCommands::Initialize: initialize(messagePointer); break;
		case FSCommands::InitializeWithSdkVersion: initializeWithSdkVersion(messagePointer); break;
		case FSCommands::IsSdmcDetected: isSdmcDetected(messagePointer); break;
		case FSCommands::IsSdmcWritable: isSdmcWritable(messagePointer); break;
		case FSCommands::OpenArchive: openArchive(messagePointer); break;
		case FSCommands::OpenDirectory: openDirectory(messagePointer); break;
		case FSCommands::OpenFile: [[likely]] openFile(messagePointer); break;
		case FSCommands::OpenFileDirectly: [[likely]] openFileDirectly(messagePointer); break;
		case FSCommands::SetPriority: setPriority(messagePointer); break;
		case FSCommands::TheGameboyVCFunction: theGameboyVCFunction(messagePointer); break;
		default: Helpers::panic("FS service requested. Command: %08X\n", command);
	}
}

void FSService::initialize(u32 messagePointer) {
	log("FS::Initialize\n");
	mem.write32(messagePointer, IPC::responseHeader(0x801, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

// TODO: Figure out how this is different from Initialize
void FSService::initializeWithSdkVersion(u32 messagePointer) {
	const auto version = mem.read32(messagePointer + 4);
	log("FS::InitializeWithSDKVersion(version = %d)\n", version);

	mem.write32(messagePointer, IPC::responseHeader(0x861, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void FSService::closeArchive(u32 messagePointer) {
	const Handle handle = static_cast<u32>(mem.read64(messagePointer + 4)); // TODO: archive handles should be 64-bit
	const auto object = kernel.getObject(handle, KernelObjectType::Archive);
	log("FSService::CloseArchive(handle = %X)\n", handle);

	mem.write32(messagePointer, IPC::responseHeader(0x80E, 1, 0));

	if (object == nullptr) {
		log("FSService::CloseArchive: Tried to close invalid archive %X\n", handle);
		mem.write32(messagePointer + 4, Result::FailurePlaceholder);
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

	Rust::Result<Handle, Result::HorizonResult> res = openArchiveHandle(archiveID, archivePath);
	mem.write32(messagePointer, IPC::responseHeader(0x80C, 3, 0));
	if (res.isOk()) {
		mem.write32(messagePointer + 4, Result::Success);
		mem.write64(messagePointer + 8, res.unwrap());
	} else {
		log("FS::OpenArchive: Failed to open archive with id = %d. Error %08X\n", archiveID, (u32)res.unwrapErr());
		mem.write32(messagePointer + 4, res.unwrapErr());
		mem.write64(messagePointer + 8, 0);
	}
}

void FSService::openFile(u32 messagePointer) {
	const Handle archiveHandle = Handle(mem.read64(messagePointer + 8));
	const u32 filePathType = mem.read32(messagePointer + 16);
	const u32 filePathSize = mem.read32(messagePointer + 20);
	const u32 openFlags = mem.read32(messagePointer + 24);
	const u32 attributes = mem.read32(messagePointer + 28);
	const u32 filePathPointer = mem.read32(messagePointer + 36);

	log("FS::OpenFile\n");

	auto archiveObject = kernel.getObject(archiveHandle, KernelObjectType::Archive);
	if (archiveObject == nullptr) [[unlikely]] {
		log("FS::OpenFile: Invalid archive handle %d\n", archiveHandle);
		mem.write32(messagePointer + 4, Result::FailurePlaceholder);
		return;
	}

	ArchiveBase* archive = archiveObject->getData<ArchiveSession>()->archive;
	const FSPath& archivePath = archiveObject->getData<ArchiveSession>()->path;

	auto filePath = readPath(filePathType, filePathPointer, filePathSize);
	const FilePerms perms(openFlags);

	std::optional<Handle> handle = openFileHandle(archive, filePath, archivePath, perms);
	mem.write32(messagePointer, IPC::responseHeader(0x802, 1, 2));
	if (!handle.has_value()) {
		printf("OpenFile failed\n");
		mem.write32(messagePointer + 4, Result::FS::FileNotFound);
	} else {
		mem.write32(messagePointer + 4, Result::Success);
		mem.write32(messagePointer + 8, 0x10); // "Move handle descriptor"
		mem.write32(messagePointer + 12, handle.value());
	}
}

void FSService::createDirectory(u32 messagePointer) {
	log("FS::CreateDirectory\n");

	const Handle archiveHandle = (Handle)mem.read64(messagePointer + 8);
	const u32 pathType = mem.read32(messagePointer + 16);
	const u32 pathSize = mem.read32(messagePointer + 20);
	const u32 pathPointer = mem.read32(messagePointer + 32);

	KernelObject* archiveObject = kernel.getObject(archiveHandle, KernelObjectType::Archive);
	if (archiveObject == nullptr) [[unlikely]] {
		log("FS::CreateDirectory: Invalid archive handle %d\n", archiveHandle);
		mem.write32(messagePointer + 4, Result::FailurePlaceholder);
		return;
	}

	ArchiveBase* archive = archiveObject->getData<ArchiveSession>()->archive;
	const auto dirPath = readPath(pathType, pathPointer, pathSize);
	const Result::HorizonResult res = archive->createDirectory(dirPath);

	mem.write32(messagePointer, IPC::responseHeader(0x809, 1, 0));
	mem.write32(messagePointer + 4, static_cast<u32>(res));
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
		mem.write32(messagePointer + 4, Result::FailurePlaceholder);
		return;
	}

	ArchiveBase* archive = archiveObject->getData<ArchiveSession>()->archive;
	const auto dirPath = readPath(pathType, pathPointer, pathSize);
	auto dir = openDirectoryHandle(archive, dirPath);

	mem.write32(messagePointer, IPC::responseHeader(0x80B, 1, 2));
	if (dir.isOk()) {
		mem.write32(messagePointer + 4, Result::Success);
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

	Rust::Result<ArchiveBase*, Result::HorizonResult> res = archive->openArchive(archivePath);
	if (res.isErr()) [[unlikely]] {
		Helpers::panic("OpenFileDirectly: Failed to open archive with given path");
	}
	archive = res.unwrap();

	std::optional<Handle> handle = openFileHandle(archive, filePath, archivePath, perms);
	mem.write32(messagePointer, IPC::responseHeader(0x803, 1, 2));
	if (!handle.has_value()) {
		Helpers::panic("OpenFileDirectly: Failed to open file with given path");
	} else {
		mem.write32(messagePointer + 4, Result::Success);
		mem.write32(messagePointer + 12, handle.value());
	}
}

void FSService::createFile(u32 messagePointer) {
	const Handle archiveHandle = Handle(mem.read64(messagePointer + 8));
	const u32 filePathType = mem.read32(messagePointer + 16);
	const u32 filePathSize = mem.read32(messagePointer + 20);
	const u32 attributes = mem.read32(messagePointer + 24);
	const u64 size = mem.read64(messagePointer + 28);
	const u32 filePathPointer = mem.read32(messagePointer + 40);

	log("FS::CreateFile\n");

	auto archiveObject = kernel.getObject(archiveHandle, KernelObjectType::Archive);
	if (archiveObject == nullptr) [[unlikely]] {
		log("FS::OpenFile: Invalid archive handle %d\n", archiveHandle);
		mem.write32(messagePointer + 4, Result::FailurePlaceholder);
		return;
	}

	ArchiveBase* archive = archiveObject->getData<ArchiveSession>()->archive;
	auto filePath = readPath(filePathType, filePathPointer, filePathSize);

	Result::HorizonResult res = archive->createFile(filePath, size);
	mem.write32(messagePointer, IPC::responseHeader(0x808, 1, 0));
	mem.write32(messagePointer + 4, res);
}

void FSService::deleteFile(u32 messagePointer) {
	const Handle archiveHandle = Handle(mem.read64(messagePointer + 8));
	const u32 filePathType = mem.read32(messagePointer + 16);
	const u32 filePathSize = mem.read32(messagePointer + 20);
	const u32 filePathPointer = mem.read32(messagePointer + 28);

	log("FS::DeleteFile\n");
	auto archiveObject = kernel.getObject(archiveHandle, KernelObjectType::Archive);
	if (archiveObject == nullptr) [[unlikely]] {
		log("FS::DeleteFile: Invalid archive handle %d\n", archiveHandle);
		mem.write32(messagePointer + 4, Result::FailurePlaceholder);
		return;
	}

	ArchiveBase* archive = archiveObject->getData<ArchiveSession>()->archive;
	auto filePath = readPath(filePathType, filePathPointer, filePathSize);

	Result::HorizonResult res = archive->deleteFile(filePath);
	mem.write32(messagePointer, IPC::responseHeader(0x804, 1, 0));
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

	mem.write32(messagePointer, IPC::responseHeader(0x845, 5, 0));
	Rust::Result<ArchiveBase::FormatInfo, Result::HorizonResult> res = archive->getFormatInfo(path);

	// If the FormatInfo was returned, write them to the output buffer. Otherwise, write an error code.
	if (res.isOk()) {
		ArchiveBase::FormatInfo info = res.unwrap();
		mem.write32(messagePointer + 4, Result::Success);
		mem.write32(messagePointer + 8, info.size);
		mem.write32(messagePointer + 12, info.numOfDirectories);
		mem.write32(messagePointer + 16, info.numOfFiles);
		mem.write8(messagePointer + 20, info.duplicateData ? 1 : 0);
	} else {
		mem.write32(messagePointer + 4, static_cast<u32>(res.unwrapErr()));
	}
}

void FSService::formatSaveData(u32 messagePointer) {
	log("FS::FormatSaveData\n");

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

	ArchiveBase::FormatInfo info {
		.size = blockSize * 0x200,
		.numOfDirectories = directoryNum,
		.numOfFiles = fileNum,
		.duplicateData = duplicateData
	};

	saveData.format(path, info);

	mem.write32(messagePointer, IPC::responseHeader(0x84C, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void FSService::deleteExtSaveData(u32 messagePointer) {
	Helpers::warn("Stubbed call to FS::DeleteExtSaveData!");
	// First 4 words of parameters are the ExtSaveData info
	// https://www.3dbrew.org/wiki/Filesystem_services#ExtSaveDataInfo
	const u8 mediaType = mem.read8(messagePointer + 4);
	const u64 saveID = mem.read64(messagePointer + 8);
	log("FS::DeleteExtSaveData (media type = %d, saveID = %llx) (stubbed)\n", mediaType, saveID);

	mem.write32(messagePointer, IPC::responseHeader(0x0852, 1, 0));
	// TODO: We can't properly implement this yet until we properly support title/save IDs. We will stub this and insert a warning for now. Required for Planet Robobot
	// When we properly implement it, it will just be a recursive directory deletion
	mem.write32(messagePointer + 4, Result::Success);
}

void FSService::createExtSaveData(u32 messagePointer) {
	Helpers::warn("Stubbed call to FS::CreateExtSaveData!");
	// First 4 words of parameters are the ExtSaveData info
	// https://www.3dbrew.org/wiki/Filesystem_services#ExtSaveDataInfo
	// This creates the ExtSaveData with the specified saveid in the specified media type. It stores the SMDH as "icon" in the root of the created directory. 
	const u8 mediaType = mem.read8(messagePointer + 4);
	const u64 saveID = mem.read64(messagePointer + 8);
	const u32 numOfDirectories = mem.read32(messagePointer + 20);
	const u32 numOfFiles = mem.read32(messagePointer + 24);
	const u64 sizeLimit = mem.read64(messagePointer + 28);
	const u32 smdhSize = mem.read32(messagePointer + 36);
	const u32 smdhPointer = mem.read32(messagePointer + 44);

	log("FS::CreateExtSaveData (stubbed)\n");

	mem.write32(messagePointer, IPC::responseHeader(0x0851, 1, 0));
	// TODO: Similar to DeleteExtSaveData, we need to refactor how our ExtSaveData stuff works before properly implementing this
	mem.write32(messagePointer + 4, Result::Success);
}

void FSService::formatThisUserSaveData(u32 messagePointer) {
	log("FS::FormatThisUserSaveData\n");

	const u32 blockSize = mem.read32(messagePointer + 4);
	const u32 directoryNum = mem.read32(messagePointer + 8); // Max number of directories
	const u32 fileNum = mem.read32(messagePointer + 12); // Max number of files
	const u32 directoryBucketNum = mem.read32(messagePointer + 16); // Not sure what a directory bucket is...?
	const u32 fileBucketNum = mem.read32(messagePointer + 20); // Same here
	const bool duplicateData = mem.read8(messagePointer + 24) != 0;

	ArchiveBase::FormatInfo info {
		.size = blockSize * 0x200,
		.numOfDirectories = directoryNum,
		.numOfFiles = fileNum,
		.duplicateData = duplicateData
	};
	FSPath emptyPath;

	mem.write32(messagePointer, IPC::responseHeader(0x080F, 1, 0));
	saveData.format(emptyPath, info);
}

void FSService::controlArchive(u32 messagePointer) {
	const Handle archiveHandle = Handle(mem.read64(messagePointer + 4));
	const u32 action = mem.read32(messagePointer + 12);
	const u32 inputSize = mem.read32(messagePointer + 16);
	const u32 outputSize = mem.read32(messagePointer + 20);
	const u32 input = mem.read32(messagePointer + 28);
	const u32 output = mem.read32(messagePointer + 36);

	log("FS::ControlArchive (action = %X, handle = %X)\n", action, archiveHandle);

	auto archiveObject = kernel.getObject(archiveHandle, KernelObjectType::Archive);
	mem.write32(messagePointer, IPC::responseHeader(0x80D, 1, 0));
	if (archiveObject == nullptr) [[unlikely]] {
		log("FS::ControlArchive: Invalid archive handle %d\n", archiveHandle);
		mem.write32(messagePointer + 4, Result::FailurePlaceholder);
		return;
	}

	switch (action) {
		case 0: // Commit save data changes. Shouldn't need us to do anything
			mem.write32(messagePointer + 4, Result::Success);
			break;

		case 1: // Retrieves a file's last-modified timestamp. Seen in DDLC, stubbed for the moment
			Helpers::warn("FS::ControlArchive: Tried to retrieve a file's last-modified timestamp");
			mem.write32(messagePointer + 4, Result::Success);
			break;

		default:
			Helpers::panic("Unimplemented action for ControlArchive (action = %X)\n", action);
			break;
	}
}

void FSService::getFreeBytes(u32 messagePointer) {
	log("FS::GetFreeBytes\n");
	const Handle archiveHandle = (Handle)mem.read64(messagePointer + 4);
	auto session = kernel.getObject(archiveHandle, KernelObjectType::Archive);

	mem.write32(messagePointer, IPC::responseHeader(0x812, 3, 0));
	if (session == nullptr) [[unlikely]] {
		log("FS::GetFreeBytes: Invalid archive handle %d\n", archiveHandle);
		mem.write32(messagePointer + 4, Result::FailurePlaceholder);
		return;
	}

	const u64 bytes = session->getData<ArchiveSession>()->archive->getFreeBytes();
	mem.write64(messagePointer + 8, bytes);
}

void FSService::getPriority(u32 messagePointer) {
	log("FS::GetPriority\n");

	mem.write32(messagePointer, IPC::responseHeader(0x863, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, priority);
}

void FSService::setPriority(u32 messagePointer) {
	const u32 value = mem.read32(messagePointer + 4);
	log("FS::SetPriority (priority = %d)\n", value);

	mem.write32(messagePointer, IPC::responseHeader(0x862, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
	priority = value;
}

void FSService::getThisSaveDataSecureValue(u32 messagePointer) {
	Helpers::warn("Unimplemented FS::GetThisSaveDataSecureValue");

	mem.write32(messagePointer, IPC::responseHeader(0x86F, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void FSService::theGameboyVCFunction(u32 messagePointer) {
	Helpers::warn("Unimplemented FS: function: 0x08750180");

	mem.write32(messagePointer, IPC::responseHeader(0x875, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

// Shows whether an SD card is inserted. At the moment stubbed to no
constexpr bool sdInserted = false;

void FSService::isSdmcDetected(u32 messagePointer) {
	log("FS::IsSdmcDetected\n");
	mem.write32(messagePointer, IPC::responseHeader(0x817, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, sdInserted ? 1 : 0);
}

// We consider our SD card to always be writable if oen is inserted for now
// So isSdmcWritable returns 1 if an SD card is inserted (because it's always writable) and 0 if not.
void FSService::isSdmcWritable(u32 messagePointer) {
	log("FS::isSdmcWritable\n");
	mem.write32(messagePointer, IPC::responseHeader(0x818, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, sdInserted ? 1 : 0);
}