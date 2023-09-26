#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

class LCDService {
	Handle handle = KernelHandles::LCD;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, gspLCDLogger)

	// Service commands
	void setLedForceOff(u32 messagePointer);

public:
	LCDService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};