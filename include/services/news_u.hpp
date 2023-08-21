#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

class NewsUService {
	Handle handle = KernelHandles::NEWS_U;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, newsLogger)

	// Service commands

  public:
	NewsUService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};