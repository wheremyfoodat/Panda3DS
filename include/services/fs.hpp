#pragma once
#include "fs/archive_ncch.hpp"
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

	SelfNCCHArchive selfNcch;

	ArchiveBase* getArchiveFromID(u32 id);
	std::optional<Handle> openFile(ArchiveBase* archive, const FSPath& path);

	// Service commands
	void initialize(u32 messagePointer);
	void openArchive(u32 messagePointer);
	void openFileDirectly(u32 messagePointer);

public:
	FSService(Memory& mem, Kernel& kernel) : mem(mem), selfNcch(mem), kernel(kernel) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};