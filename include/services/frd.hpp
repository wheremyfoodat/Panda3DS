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
	Memory& mem;
	MAKE_LOG_FUNCTION(log, frdLogger)

	bool loggedIn = false;

	// Service commands
	void attachToEventNotification(u32 messagePointer);
	void getFriendAttributeFlags(u32 messagePointer);
	void getFriendKeyList(u32 messagePointer);
	void getFriendPresence(u32 messagePointer);
	void getFriendProfile(u32 messagePointer);
	void getMyComment(u32 messagePointer);
	void getMyFavoriteGame(u32 messagePointer);
	void getMyFriendKey(u32 messagePointer);
	void getMyMii(u32 messagePointer);
	void getMyPresence(u32 messagePointer);
	void getMyProfile(u32 messagePointer);
	void getMyScreenName(u32 messsagePointer);
	void hasLoggedIn(u32 messagePointer);
	void isOnline(u32 messagePointer);
	void logout(u32 messagePointer);
	void setClientSDKVersion(u32 messagePointer);
	void setNotificationMask(u32 messagePointer);
	void updateGameModeDescription(u32 messagePointer);
	void updateMii(u32 messagePointer);

	struct Profile {
		u8 region;
		u8 country;
		u8 area;
		u8 language;
		u32 unknown;
	};
	static_assert(sizeof(Profile) == 8);

public:
	enum class Type {
		A,    // frd:a
		N,    // frd:n
		U,    // frd:u
	};

	FRDService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer, Type type);
};