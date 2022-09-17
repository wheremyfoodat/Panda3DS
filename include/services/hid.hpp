#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "memory.hpp"

class HIDService {
	Handle handle = KernelHandles::HID;
	Memory& mem;

	// Service commands
	void getIPCHandles(u32 messagePointer);

public:
	HIDService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};