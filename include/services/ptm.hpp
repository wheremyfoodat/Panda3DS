#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

class PTMService {
	Handle handle = KernelHandles::PTM;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, ptmLogger)

	// Service commands
	void getStepHistory(u32 messagePointer);
	void getTotalStepCount(u32 messagePointer);

public:
	PTMService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};