#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

class CAMService {
	Handle handle = KernelHandles::CAM;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, camLogger)

	// Service commands
	void driverInitialize(u32 messagePointer);

public:
	CAMService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};