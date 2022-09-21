#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "memory.hpp"

class NDMService {
	Handle handle = KernelHandles::NDM;
	Memory& mem;

	// Service commands
	void overrideDefaultDaemons(u32 messagePointer);
	void suspendDaemons(u32 messagePointer);

public:
	NDMService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};