#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

class SOCService {
	HandleType handle = KernelHandles::SOC;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, socLogger)

	bool initialized = false;

	// Service commands
	void initializeSockets(u32 messagePointer);

  public:
	SOCService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
