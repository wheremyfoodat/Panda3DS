#pragma once
#include "fs/archive_ncch.hpp"
#include "fs/archive_save_data.hpp"
#include "fs/archive_sdmc.hpp"
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

	// The different filesystem archives (Save data, RomFS+ExeFS, etc)
	SelfNCCHArchive selfNcch;
	SaveDataArchive saveData;
	SDMCArchive sdmc;

	ArchiveBase* getArchiveFromID(u32 id);
	std::optional<Handle> openArchiveHandle(u32 archiveID, const FSPath& path);
	std::optional<Handle> openFileHandle(ArchiveBase* archive, const FSPath& path);

	// Service commands
	void closeArchive(u32 messagePointer);
	void initialize(u32 messagePointer);
	void openArchive(u32 messagePointer);
	void openFile(u32 messagePointer);
	void openFileDirectly(u32 messagePointer);

public:
	FSService(Memory& mem, Kernel& kernel) : mem(mem), saveData(mem), sdmc(mem), selfNcch(mem), kernel(kernel) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};