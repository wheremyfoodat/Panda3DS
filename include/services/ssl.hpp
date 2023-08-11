#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

class SSLService {
	Handle handle = KernelHandles::SSL;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, sslLogger)

	// Service commands

  public:
	SSLService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};