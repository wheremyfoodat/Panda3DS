#include "services/frd.hpp"

#include <string>

#include "ipc.hpp"
#include "services/region_codes.hpp"

namespace FRDCommands {
	enum : u32 {
		HasLoggedIn = 0x00010000,
		IsOnline = 0x00020000,
		AttachToEventNotification = 0x00200002,
		SetNotificationMask = 0x00210040,
		SetClientSdkVersion = 0x00320042,
		Logout = 0x00040000,
		GetMyFriendKey = 0x00050000,
		GetMyProfile = 0x00070000,
		GetMyPresence = 0x00080000,
		GetMyScreenName = 0x00090000,
		GetMyMii = 0x000A0000,
		GetMyFavoriteGame = 0x000D0000,
		GetMyComment = 0x000F0000,
		GetFriendKeyList = 0x00110080,
		GetFriendPresence = 0x00120042,
		GetFriendProfile = 0x00150042,
		GetFriendRelationship = 0x00160042,
		GetFriendAttributeFlags = 0x00170042,
		UpdateGameModeDescription = 0x001D0002,

		UpdateMii = 0x040C0800,
	};
}

void FRDService::reset() { loggedIn = false; }

void FRDService::handleSyncRequest(u32 messagePointer, FRDService::Type type) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case FRDCommands::AttachToEventNotification: attachToEventNotification(messagePointer); break;
		case FRDCommands::GetFriendAttributeFlags: getFriendAttributeFlags(messagePointer); break;
		case FRDCommands::GetFriendKeyList: getFriendKeyList(messagePointer); break;
		case FRDCommands::GetFriendPresence: getFriendPresence(messagePointer); break;
		case FRDCommands::GetFriendProfile: getFriendProfile(messagePointer); break;
		case FRDCommands::GetFriendRelationship: getFriendRelationship(messagePointer); break;
		case FRDCommands::GetMyComment: getMyComment(messagePointer); break;
		case FRDCommands::GetMyFriendKey: getMyFriendKey(messagePointer); break;
		case FRDCommands::GetMyMii: getMyMii(messagePointer); break;
		case FRDCommands::GetMyFavoriteGame: getMyFavoriteGame(messagePointer); break;
		case FRDCommands::GetMyPresence: getMyPresence(messagePointer); break;
		case FRDCommands::GetMyProfile: getMyProfile(messagePointer); break;
		case FRDCommands::GetMyScreenName: getMyScreenName(messagePointer); break;
		case FRDCommands::HasLoggedIn: hasLoggedIn(messagePointer); break;
		case FRDCommands::IsOnline: isOnline(messagePointer); break;
		case FRDCommands::Logout: logout(messagePointer); break;
		case FRDCommands::SetClientSdkVersion: setClientSDKVersion(messagePointer); break;
		case FRDCommands::SetNotificationMask: setNotificationMask(messagePointer); break;
		case FRDCommands::UpdateGameModeDescription: updateGameModeDescription(messagePointer); break;

		default: 
			// FRD:A functions
			if (type == Type::A) {
				switch (command) {
					case FRDCommands::UpdateMii: updateMii(messagePointer); break;
					default: Helpers::panic("FRD:A service requested. Command: %08X\n", command); break;
				}
			} else {
				Helpers::panic("FRD service requested. Command: %08X\n", command);
			}

			break;
	}
}

void FRDService::attachToEventNotification(u32 messagePointer) {
	log("FRD::AttachToEventNotification (Undocumented)\n");
	mem.write32(messagePointer + 4, Result::Success);
}

// This is supposed to post stuff on your user profile so uhh can't really emulate it
void FRDService::updateGameModeDescription(u32 messagePointer) {
	log("FRD::UpdateGameModeDescription\n");

	mem.write32(messagePointer, IPC::responseHeader(0x1D, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void FRDService::getMyFriendKey(u32 messagePointer) {
	log("FRD::GetMyFriendKey\n");

	mem.write32(messagePointer, IPC::responseHeader(0x5, 5, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);  // Principal ID
	mem.write32(messagePointer + 12, 0); // Padding (?)
	mem.write32(messagePointer + 16, 0); // Local friend code
	mem.write32(messagePointer + 20, 0);
}

void FRDService::getFriendKeyList(u32 messagePointer) {
	log("FRD::GetFriendKeyList\n");

	const u32 count = mem.read32(messagePointer + 8); // From what I understand this is a cap on the number of keys to receive?
	constexpr u32 friendCount = 0; // And this should be the number of friends whose keys were actually received?

	mem.write32(messagePointer, IPC::responseHeader(0x11, 2, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, friendCount);

	// Zero out friend keys
	for (u32 i = 0; i < count * sizeof(FriendKey); i += 4) {
		mem.write32(messagePointer + 12 + i, 0);
	}
}

void FRDService::getFriendProfile(u32 messagePointer) {
	log("FRD::GetFriendProfile\n");

	const u32 count = mem.read32(messagePointer + 4);
	const u32 friendKeyList = mem.read32(messagePointer + 12);  // Pointer to list of friend keys
	const u32 profile = mem.read32(messagePointer + 0x104);     // Pointer to friend profile where we'll write info to

	mem.write32(messagePointer, IPC::responseHeader(0x15, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);

	// Clear all profiles
	for (u32 i = 0; i < count; i++) {
		const u32 pointer = profile + (i * sizeof(Profile));
		for (u32 j = 0; j < sizeof(Profile); j++) {
			mem.write8(pointer + j, 0);
		}
	}
}

void FRDService::getFriendAttributeFlags(u32 messagePointer) {
	log("FRD::GetFriendAttributeFlags\n");

	const u32 count = mem.read32(messagePointer + 4);
	const u32 friendKeyList = mem.read32(messagePointer + 12);  // Pointer to list of friend keys
	const u32 profile = mem.read32(messagePointer + 0x104);     // Pointer to friend profile where we'll write info to

	mem.write32(messagePointer, IPC::responseHeader(0x17, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);

	// Clear flags
	for (u32 i = 0; i < count; i++) {
		mem.write8(profile + i, 0);
	}
}

void FRDService::getMyPresence(u32 messagePointer) {
	static constexpr u32 presenceSize = 0x12C; // A presence seems to be 12C bytes of data, not sure what it contains
	log("FRD::GetMyPresence\n");
	u32 buffer = mem.read32(messagePointer + 0x104); // Buffer to write presence info to.

	for (u32 i = 0; i < presenceSize; i += 4) { // Clear presence info with 0s for now
		mem.write32(buffer + i, 0);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x8, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
}

void FRDService::getFriendPresence(u32 messagePointer) {
	Helpers::warn("FRD::GetFriendPresence (stubbed)");

	// TODO: Implement and document this,
	mem.write32(messagePointer, IPC::responseHeader(0x12, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void FRDService::getMyProfile(u32 messagePointer) {
	mem.write32(messagePointer, IPC::responseHeader(0x7, 3, 0)); // Not sure if the header here has the correct # of responses?
	mem.write32(messagePointer + 4, Result::Success);

	// TODO: Should maybe make these user-configurable. Not super important though
	mem.write8(messagePointer + 8, static_cast<u8>(Regions::USA));            // Region
	mem.write8(messagePointer + 9, static_cast<u8>(CountryCodes::US));        // Country
	mem.write8(messagePointer + 10, 2);                                       // Area (this should be Washington)
	mem.write8(messagePointer + 11, static_cast<u8>(LanguageCodes::English)); // Language
	mem.write8(messagePointer + 12, 2);                                       // Platform (always 2 for CTR)

	// Padding
	mem.write8(messagePointer + 13, 0);
	mem.write8(messagePointer + 14, 0);
	mem.write8(messagePointer + 15, 0);
}

void FRDService::getMyScreenName(u32 messagePointer) {
	log("FRD::GetMyScreenName\n");
	static const std::u16string name = u"Pander";
	mem.write32(messagePointer + 4, Result::Success);

	// TODO: Assert the name fits in the response buffer
	u32 pointer = messagePointer + 8;
	for (auto c : name) {
		mem.write16(pointer, static_cast<u16>(c));
		pointer += sizeof(u16);
	}

	// Add null terminator
	mem.write16(pointer, static_cast<u16>(u'\0'));
}

void FRDService::setClientSDKVersion(u32 messagePointer) {
	u32 version = mem.read32(messagePointer + 4);
	log("FRD::SetClientSdkVersion (version = %d)\n", version);

	mem.write32(messagePointer, IPC::responseHeader(0x32, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void FRDService::setNotificationMask(u32 messagePointer) {
	log("FRD::SetNotificationMask (Not documented)\n");

	mem.write32(messagePointer, IPC::responseHeader(0x21, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void FRDService::getMyMii(u32 messagePointer) {
	log("FRD::GetMyMii (stubbed)\n");

	// TODO: How is the mii data even returned?
	mem.write32(messagePointer, IPC::responseHeader(0xA, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void FRDService::getMyFavoriteGame(u32 messagePointer) {
	log("FRD::GetMyFavoriteGame (stubbed)\n");
	constexpr u64 titleID = 0;
	
	mem.write32(messagePointer, IPC::responseHeader(0xD, 3, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write64(messagePointer + 8, titleID);
}

void FRDService::getMyComment(u32 messagePointer) {
	log("FRD::GetMyComment");

	mem.write32(messagePointer, IPC::responseHeader(0xF, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);
}

void FRDService::hasLoggedIn(u32 messagePointer) {
	log("FRD::HasLoggedIn\n");

	mem.write32(messagePointer, IPC::responseHeader(0x1, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, loggedIn ? 1 : 0);
}

void FRDService::isOnline(u32 messagePointer) {
	log("FRD::IsOnline\n");

	mem.write32(messagePointer, IPC::responseHeader(0x2, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	// TODO: When is this 0?
	mem.write8(messagePointer + 8, 1);
}

void FRDService::logout(u32 messagePointer) {
	log("FRD::Logout\n");
	loggedIn = false;

	mem.write32(messagePointer, IPC::responseHeader(0x4, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void FRDService::updateMii(u32 messagePointer) {
	log("FRD::UpdateMii (stubbed)\n");

	mem.write32(messagePointer, IPC::responseHeader(0x40C, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void FRDService::getFriendRelationship(u32 messagePointer) {
	log("FRD::GetFriendRelationship (stubbed)\n");

	// TODO: What does this return?
	mem.write32(messagePointer, IPC::responseHeader(0x16, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}