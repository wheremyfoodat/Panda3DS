#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

class NewsSService {
	Handle handle = KernelHandles::NEWS_S;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, newsLogger)

	// Service commands

  public:
	NewsSService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};