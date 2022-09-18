#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "memory.hpp"

class GPUService {
	Handle handle = KernelHandles::GPU;
	Memory& mem;

	// Service commands

public:
	GPUService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};