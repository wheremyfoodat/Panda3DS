#pragma once
#include "helpers.hpp"
#include "logger.hpp"
#include "memory.hpp"

class DSPService {
	Handle handle = KernelHandles::DSP;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, dspServiceLogger)

public:
	DSPService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};