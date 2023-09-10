#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

class NDMService {
	Handle handle = KernelHandles::NDM;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, ndmLogger)

	// Service commands
	void clearHalfAwakeMacFilter(u32 messagePointer);
	void enterExclusiveState(u32 messagePointer);
	void exitExclusiveState(u32 messagePointer);
	void overrideDefaultDaemons(u32 messagePointer);
	void resumeDaemons(u32 messagePointer);
	void resumeScheduler(u32 messagePointer);
	void suspendDaemons(u32 messagePointer);
	void suspendScheduler(u32 messagePointer);

public:
	NDMService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};