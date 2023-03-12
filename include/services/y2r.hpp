#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

class Y2RService {
	Handle handle = KernelHandles::Y2R;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, y2rLogger)

	// Service commands

public:
	Y2RService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};