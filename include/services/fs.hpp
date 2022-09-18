#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "memory.hpp"

class FSService {
	Handle handle = KernelHandles::FS;
	Memory& mem;

	// Service commands
	void initialize(u32 messagePointer);
	void openArchive(u32 messagePointer);

public:
	FSService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};