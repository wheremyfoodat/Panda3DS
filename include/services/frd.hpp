#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

class FRDService {
	Handle handle = KernelHandles::FRD;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, frdLogger)

	// Service commands
	void getMyFriendKey(u32 messagePointer);
	void getMyPresence(u32 messagePointer);
	void setClientSDKVersion(u32 messagePointer);

public:
	FRDService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};