#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "memory.hpp"

class GPUService {
	Handle handle = KernelHandles::GPU;
	Memory& mem;
	u32& currentPID; // Process ID of the current process

	// At any point in time only 1 process has privileges to use rendering functions
	// This is the PID of that process
	u32 privilegedProcess;

	// Service commands
	void acquireRight(u32 messagePointer);
	void registerInterruptRelayQueue(u32 messagePointer);
	void writeHwRegs(u32 messagePointer);
	void writeHwRegsWithMask(u32 messagePointer);

public:
	GPUService(Memory& mem, u32& currentPID) : mem(mem), currentPID(currentPID) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};