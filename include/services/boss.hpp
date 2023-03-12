#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

class BOSSService {
	Handle handle = KernelHandles::BOSS;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, bossLogger)

	// Service commands
	void initializeSession(u32 messagePointer);
	void getOptoutFlag(u32 messagePointer);

	s8 optoutFlag;
public:
	BOSSService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};