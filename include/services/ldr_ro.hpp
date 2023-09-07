#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

class LDRService {
	Handle handle = KernelHandles::LDR_RO;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, ldrLogger)

	u32 loadedCRS;

	// Service commands
	void initialize(u32 messagePointer);
	void loadCRR(u32 messagePointer);
	void loadCRONew(u32 messagePointer);

public:
	LDRService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};