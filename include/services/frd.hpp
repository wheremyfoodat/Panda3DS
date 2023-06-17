#pragma once
#include <cassert>
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

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
	void attachToEventNotification(u32 messagePointer);
	void getFriendKeyList(u32 messagePointer);
	void getMyFriendKey(u32 messagePointer);
	void getMyMii(u32 messagePointer);
	void getMyPresence(u32 messagePointer);
	void getMyProfile(u32 messagePointer);
	void getMyScreenName(u32 messsagePointer);
	void setClientSDKVersion(u32 messagePointer);
	void setNotificationMask(u32 messagePointer);

public:
	FRDService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};