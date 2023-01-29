#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

class NDMService {
	Handle handle = KernelHandles::NDM;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, ndmLogger)

	// Service commands
	void overrideDefaultDaemons(u32 messagePointer);
	void resumeScheduler(u32 messagePointer);
	void suspendDaemons(u32 messagePointer);
	void suspendScheduler(u32 messagePointer);

public:
	NDMService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};