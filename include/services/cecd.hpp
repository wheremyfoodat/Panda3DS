#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

class CECDService {
	Handle handle = KernelHandles::CECD;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, cecdLogger)

	// Service commands
	void getEventHandle(u32 messagePointer);

public:
	CECDService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};