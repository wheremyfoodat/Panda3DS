#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

class LCDService {
	HandleType handle = KernelHandles::LCD;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, gspLCDLogger)

	// Service commands

  public:
	LCDService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
