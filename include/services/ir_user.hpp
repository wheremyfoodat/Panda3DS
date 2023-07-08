#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

class IRUserService {
	Handle handle = KernelHandles::IR_USER;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, irUserLogger)

	// Service commands

  public:
	IRUserService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};