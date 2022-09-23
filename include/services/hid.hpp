#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

class HIDService {
	Handle handle = KernelHandles::HID;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, hidLogger)

	// Service commands
	void getIPCHandles(u32 messagePointer);

public:
	HIDService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};