#pragma once
#include "types.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

class CAMService {
	Handle handle = KernelHandles::CAM;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, camLogger)

	// Service commands
	void driverInitialize(u32 messagePointer);
	void getMaxLines(u32 messagePointer);

public:
	CAMService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};