#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

class ACTService {
	HandleType handle = KernelHandles::ACT;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, actLogger)

	// Service commands
	void initialize(u32 messagePointer);
	void generateUUID(u32 messagePointer);
	void getAccountDataBlock(u32 messagePointer);

  public:
	ACTService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
