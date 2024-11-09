#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

class AMService {
	using Handle = HorizonHandle;

	Handle handle = KernelHandles::AM;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, amLogger)

	// Service commands
	void getDLCTitleInfo(u32 messagePointer);
	void getPatchTitleInfo(u32 messagePointer);
	void listTitleInfo(u32 messagePointer);

  public:
	AMService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};