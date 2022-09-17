#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "memory.hpp"

class APTService {
	Handle handle = KernelHandles::APT;
	Memory& mem;

	// Service commands
	void getLockHandle(u32 messagePointer);

public:
	APTService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};