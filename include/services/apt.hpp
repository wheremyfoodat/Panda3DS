#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

class APTService {
	Handle handle = KernelHandles::APT;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, aptLogger)

	// Service commands
	void getLockHandle(u32 messagePointer);
	void checkNew3DS(u32 messagePointer);

public:
	APTService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};