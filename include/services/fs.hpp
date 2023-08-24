#pragma once
#include "fs/archive_ext_save_data.hpp"
#include "fs/archive_ncch.hpp"
#include "fs/archive_save_data.hpp"
#include "fs/archive_sdmc.hpp"
#include "fs/archive_self_ncch.hpp"
#include "fs/archive_user_save_data.hpp"
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

// Yay, more circular dependencies
class Kernel;

class FSService {
	Handle handle = KernelHandles::FS;
	Memory& mem;
	Kernel& kernel;

	MAKE_LOG_FUNCTION(log, fsLogger)

	// The different filesystem archives (Save data, SelfNCCH, SDMC, NCCH, ExtData, etc)
	SelfNCCHArchive selfNcch;
	SaveDataArchive saveData;
	SDMCArchive sdmc;
	NCCHArchive ncch;

	// UserSaveData archives
	UserSaveDataArchive userSaveData1;
	UserSaveDataArchive userSaveData2;

	ExtSaveDataArchive extSaveData_sdmc;
	ExtSaveDataArchive sharedExtSaveData_nand;

	ArchiveBase* getArchiveFromID(u32 id, const FSPath& archivePath);
	Rust::Result<Handle, HorizonResult> openArchiveHandle(u32 archiveID, const FSPath& path);
	Rust::Result<Handle, HorizonResult> openDirectoryHandle(ArchiveBase* archive, const FSPath& path);
	std::optional<Handle> openFileHandle(ArchiveBase* archive, const FSPath& path, const FSPath& archivePath, const FilePerms& perms);
	FSPath readPath(u32 type, u32 pointer, u32 size);

	// Service commands
	void createDirectory(u32 messagePointer);
	void createExtSaveData(u32 messagePointer);
	void createFile(u32 messagePointer);
	void closeArchive(u32 messagePointer);
	void controlArchive(u32 messagePointer);
	void deleteExtSaveData(u32 messagePointer);
	void deleteFile(u32 messagePointer);
	void formatSaveData(u32 messagePointer);
	void formatThisUserSaveData(u32 messagePointer);
	void getFreeBytes(u32 messagePointer);
	void getFormatInfo(u32 messagePointer);
	void getPriority(u32 messagePointer);
	void initialize(u32 messagePointer);
	void initializeWithSdkVersion(u32 messagePointer);
	void isSdmcDetected(u32 messagePointer);
	void isSdmcWritable(u32 messagePOinter);
	void openArchive(u32 messagePointer);
	void openDirectory(u32 messagePointer);
	void openFile(u32 messagePointer);
	void openFileDirectly(u32 messagePointer);
	void setPriority(u32 messagePointer);

	// Used for set/get priority: Not sure what sort of priority this is referring to
	u32 priority;

public:
	FSService(Memory& mem, Kernel& kernel)
		: mem(mem), saveData(mem), sharedExtSaveData_nand(mem, "../SharedFiles/NAND", true), extSaveData_sdmc(mem, "SDMC"), sdmc(mem), selfNcch(mem),
		  ncch(mem), userSaveData1(mem, ArchiveID::UserSaveData1), userSaveData2(mem, ArchiveID::UserSaveData2), kernel(kernel) {}

	void reset();
	void handleSyncRequest(u32 messagePointer);
	// Creates directories for NAND, ExtSaveData, etc if they don't already exist. Should be executed after loading a new ROM.
	void initializeFilesystem();
};