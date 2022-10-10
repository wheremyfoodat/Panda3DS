#pragma once
#include "helpers.hpp"
#include "logger.hpp"
#include "memory.hpp"

class CFGService {
	Handle handle = KernelHandles::CFG;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, cfgLogger)

public:
	CFGService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};