#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

class Kernel;

class NIMService {
	Memory& mem;
	Kernel& kernel;
	MAKE_LOG_FUNCTION(log, nimLogger)

	std::optional<Handle> backgroundSystemUpdateEvent;

	// Service commands
	void getAutoTitleDownloadTaskInfos(u32 messagePointer);
	void getBackgroundEventForMenu(u32 messagePointer);
	void getTaskInfos(u32 messagePointer);
	void initialize(u32 messagePointer);
	void isPendingAutoTitleDownloadTasks(u32 messagePointer);

public:
	enum class Type {
		AOC,  // nim:aoc
		U,    // nim:u
	};

	NIMService(Memory& mem, Kernel& kernel) : mem(mem), kernel(kernel) {}
	void reset();
	void handleSyncRequest(u32 messagePointer, Type type);
};