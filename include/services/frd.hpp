#pragma once
#include <cassert>
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

// It's important to keep this struct to 16 bytes as we use its sizeof in the service functions in frd.cpp
struct FriendKey {
	u32 friendID;
	u32 dummy;
	u64 friendCode;
};
static_assert(sizeof(FriendKey) == 16);

class FRDService {
	Handle handle = KernelHandles::FRD;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, frdLogger)

	// Service commands
	void getFriendKeyList(u32 messagePointer);
	void getMyFriendKey(u32 messagePointer);
	void getMyPresence(u32 messagePointer);
	void setClientSDKVersion(u32 messagePointer);

public:
	FRDService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};