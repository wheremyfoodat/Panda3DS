#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

class AMService {
	Handle handle = KernelHandles::AM;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, amLogger)

	// Service commands
	void listTitleInfo(u32 messagePointer);

public:
	AMService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};