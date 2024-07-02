#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

class NIMService {
	HandleType handle = KernelHandles::NIM;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, nimLogger)

	// Service commands
	void initialize(u32 messagePointer);

  public:
	NIMService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
