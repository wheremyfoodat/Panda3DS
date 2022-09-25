#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

class HIDService {
	Handle handle = KernelHandles::HID;
	Memory& mem;
	u8* sharedMem; // Pointer to HID shared memory

	MAKE_LOG_FUNCTION(log, hidLogger)

	// Service commands
	void getIPCHandles(u32 messagePointer);

public:
	HIDService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);

	void setSharedMem(u8* ptr) {
		sharedMem = ptr;
		if (ptr != nullptr) { // Zero-fill shared memory in case the process tries to read stale service data or vice versa
			std::memset(ptr, 0xff, 0x2b0);
		}
	}
};