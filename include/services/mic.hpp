#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

class MICService {
	Handle handle = KernelHandles::MIC;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, micLogger)

	// Service commands
	void mapSharedMem(u32 messagePointer);

public:
	MICService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};