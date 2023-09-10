#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

class CSNDService {
	Handle handle = KernelHandles::CSND;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, csndLogger)

  public:
	CSNDService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};