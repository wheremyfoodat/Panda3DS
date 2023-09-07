#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

class Kernel;

class LDRService {
	Handle handle = KernelHandles::LDR_RO;
	Memory& mem;
	Kernel& kernel;
	MAKE_LOG_FUNCTION(log, ldrLogger)

	u32 loadedCRS;

	// Service commands
	void initialize(u32 messagePointer);
	void loadCRO(u32 messagePointer, bool isNew);
	void loadCRR(u32 messagePointer);
	void unloadCRO(u32 messagePointer);

public:
	LDRService(Memory& mem, Kernel& kernel) : mem(mem), kernel(kernel) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};