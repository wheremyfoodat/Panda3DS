#pragma once
#include "fs/archive_ext_save_data.hpp"
#include "fs/archive_ncch.hpp"
#include "fs/archive_save_data.hpp"
#include "fs/archive_sdmc.hpp"
#include "fs/archive_self_ncch.hpp"
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

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

	ExtSaveDataArchive extSaveData_nand;
	ExtSaveDataArchive extSaveData_cart;
	ExtSaveDataArchive sharedExtSaveData_nand;
	ExtSaveDataArchive sharedExtSaveData_cart;

	ArchiveBase* getArchiveFromID(u32 id, const FSPath& archivePath);
	std::optional<Handle> openArchiveHandle(u32 archiveID, const FSPath& path);
	Rust::Result<Handle, FSResult> openDirectoryHandle(ArchiveBase* archive, const FSPath& path);
	std::optional<Handle> openFileHandle(ArchiveBase* archive, const FSPath& path, const FSPath& archivePath, const FilePerms& perms);
	FSPath readPath(u32 type, u32 pointer, u32 size);

	// Service commands
	void createFile(u32 messagePointer);
	void closeArchive(u32 messagePointer);
	void deleteFile(u32 messagePointer);
	void formatSaveData(u32 messagePointer);
	void getFormatInfo(u32 messagePointer);
	void getPriority(u32 messagePointer);
	void initialize(u32 messagePointer);
	void initializeWithSdkVersion(u32 messagePointer);
	void isSdmcDetected(u32 messagePointer);
	void openArchive(u32 messagePointer);
	void openDirectory(u32 messagePointer);
	void openFile(u32 messagePointer);
	void openFileDirectly(u32 messagePointer);
	void setPriority(u32 messagePointer);

	// Used for set/get priority: Not sure what sort of priority this is referring to
	u32 priority;

public:
	FSService(Memory& mem, Kernel& kernel) : mem(mem), saveData(mem), extSaveData_nand(mem, "NAND"), 
		sharedExtSaveData_nand(mem, "NAND", true), extSaveData_cart(mem, "CartSave"), sharedExtSaveData_cart(mem, "CartSave", true),
		sdmc(mem), selfNcch(mem), ncch(mem), kernel(kernel)
	{}
	
	void reset();
	void handleSyncRequest(u32 messagePointer);
	// Creates directories for NAND, ExtSaveData, etc if they don't already exist. Should be executed after loading a new ROM.
	void initializeFilesystem();
};