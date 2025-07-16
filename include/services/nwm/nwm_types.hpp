#pragma once
#include <array>
#include <cstring>
#include <vector>

#include "helpers.hpp"
#include "swap.hpp"

namespace NWM {
	// The maximum number of nodes that can exist in an UDS session.
	static constexpr u32 UDSMaxNodes = 16;
	static constexpr u8 defaultNetworkChannel = 11;
	static constexpr std::array<u8, 3> nintendoOUI = {0x00, 0x1F, 0x32};

	using AESKey = std::array<u8, 16>;
	using SHA1 = std::array<u8, 20>;

	enum class ConnectionState : u32 {
		NotConnected = 3,
		ConnectedAsHost = 6,
		Connecting = 7,
		ConnectedAsClient = 9,
		ConnectedAsSpectator = 10,
	};

	enum class ConnectionStateChangeReason : u32 {
		None = 0,
		ConnectionEstablished = 1,
		ConnectionLost = 4,
	};

	struct ConnectionStatus {
		enum_le<ConnectionState> status = ConnectionState::NotConnected;
		enum_le<ConnectionStateChangeReason> statusChangeReason = ConnectionStateChangeReason::None;
		u16_le networkNodeID = 0;
		// Bitmap of which nodes have had their network status changed
		u16_le changedNodes = 0;
		u16_le nodes[UDSMaxNodes];
		// How many nodes are connected to our network at the moment?
		u8 totalNodes = 0;
		// Max number of nodes that can be connected to our network at a time
		u8 maxNodes;
		// A bitmap showing which network nodes are taken and which can be allocated.
		u16_le nodeBitmap = 0;
	};

	struct NetworkInfo {
		static constexpr usize maxApplicationDataSize = 0xC8;

		std::array<u8, 6> hostMAC;
		u8 channel;
		u8 unk1;
		u8 initialized;
		u8 unk2[3];
		// Organizationally Unique Identifier of the device
		std::array<u8, 3> oui;
		u8 ouiType;
		u32_be wlanID;
		u8 id;
		u8 unk3;
		u16_be attributes;
		u32_be networkID;
		u8 totalNodes;
		u8 maxNodes;
		u8 unk4[2];
		u8 unk5[0x1F];
		u8 applicationDataSize;
		u8 applicationData[maxApplicationDataSize];
	};
	static_assert(sizeof(NetworkInfo) == 0x108);

	// Append an object to our beacon data as raw bytes
	template <typename T>
	void append(std::vector<u8>& beaconData, const T& obj) {
		const u8* ptr = (u8*)&obj;
		const usize size = sizeof(T);

		beaconData.insert(beaconData.end(), ptr, ptr + size);
	}

	// https://github.com/azahar-emu/azahar/blob/df134acefea5b7bfe533e5333e13884528220776/src/core/hle/service/nwm/uds_beacon.h#L52
	// The pragma pack here is actually needed since the compiler will pad the struct from 12 to 16 bytes
#pragma pack(push, 1)
	struct BeaconHeader {
		u64_le timestamp = 60000000;  // Microseconds since the network was set up (Default to 1min)
		u16_le beaconInterval = 100;  // Timing units between beacon broadcasts
		u16_le capabilities = 0x431;
	};
	static_assert(sizeof(BeaconHeader) == 0xC, "UDS Beacon header has wrong size");
#pragma pack(pop)

	enum class TagID : u8 {
		SSID = 0,
		SupportedRates = 1,
		DSParameterSet = 2,
		TrafficIndicationMap = 5,
		CountryInformation = 7,
		ERPInformation = 42,
		VendorSpecific = 221
	};

	enum class VendorSpecificTagID : u8 {
		Dummy = 20,
		NetworkInfo = 21,
		EncryptedData0 = 24,
		EncryptedData1 = 25,
	};

	struct BeaconTagHeader {
		u8 tagID;
		u8 length;

		BeaconTagHeader() : tagID(u8(TagID::VendorSpecific)), length(0) {}
		BeaconTagHeader(u8 tagID, u8 length) : tagID(tagID), length(length) {}
	};
	static_assert(sizeof(BeaconTagHeader) == 2, "UDS Beacon tag header has wrong size");

	struct SSIDTag {
		BeaconTagHeader header = BeaconTagHeader(u8(TagID::SSID), sizeof(ssid));

		std::array<u8, 8> ssid = {
			0, 0, 0, 0, 0, 0, 0, 0,
		};
	};
	static_assert(sizeof(SSIDTag) == 0xA, "SSID tag has wrong size");

	struct VendorTag {
		BeaconTagHeader header = BeaconTagHeader(u8(TagID::VendorSpecific), sizeof(VendorTag) - sizeof(BeaconTagHeader));
		std::array<u8, 3> oui = nintendoOUI;
		u8 ouiType = u8(VendorSpecificTagID::Dummy);
		// https://github.com/azahar-emu/azahar/blob/df134acefea5b7bfe533e5333e13884528220776/src/core/hle/service/nwm/uds_beacon.cpp#L101
		std::array<u8, 3> data = {0x0A, 0x00, 0x00};
	};
	static_assert(sizeof(VendorTag) == 9, "NWM: Vendor tag has wrong size");

	struct NetworkInfoTag {
		BeaconTagHeader header;
		std::array<u8, 0x1F> networkInfoData;
		SHA1 sha;
		u8 applicationDataSize;

		std::vector<u8> getBytes(const NetworkInfo& networkInfo);
	};
	static_assert(sizeof(NetworkInfoTag) == 54, "NWM: Network info tag has wrong size");

	class Beacon {
		BeaconHeader header;
		SSIDTag ssidTag;
		VendorTag vendorTag;
		NetworkInfoTag networkInfoTagGenerator;

		// TODO: Other tags here

		std::vector<u8> generateCustomTags() {
			return {221, 124, 0,   31,  50,  24,  83,  96,  134, 254, 243, 210, 89,  99,  16,  90,  107, 110, 100, 82,  52,  243, 8,   41,  13,  233,
					253, 33,  244, 73,  46,  133, 238, 4,   112, 63,  161, 152, 210, 173, 226, 222, 113, 11,  47,  66,  109, 175, 175, 172, 156, 165,
					96,  242, 26,  182, 165, 202, 197, 143, 117, 27,  3,   125, 158, 42,  255, 108, 92,  21,  117, 181, 155, 0,   51,  115, 150, 210,
					227, 54,  5,   103, 70,  186, 22,  179, 171, 209, 232, 89,  222, 81,  46,  237, 105, 97,  216, 26,  31,  184, 238, 132, 201, 179,
					183, 146, 93,  162, 74,  247, 247, 111, 177, 218, 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0};
		}

	  public:
		// Converts the beacon to a vector that represents the raw bytestream
		std::vector<u8> generate(const NetworkInfo& networkInfo, AESKey& beaconKey, AESKey& iv) {
			std::vector<u8> ret;

			append(ret, header);
			append(ret, ssidTag);
			append(ret, vendorTag);

			std::vector<u8> networkInfoTag = networkInfoTagGenerator.getBytes(networkInfo);
			std::vector<u8> customTags = generateCustomTags();

			ret.insert(ret.end(), networkInfoTag.begin(), networkInfoTag.end());
			ret.insert(ret.end(), customTags.begin(), customTags.end());

			return ret;
		}
	};

#pragma pack(push, 1)
	// Node info is stored in big endian inside the UDS beacon and byteswapped to little endian by GetNodeInformationList
	struct NodeInfoFromBeacon {
		u64_be friendCodeSeed;
		std::array<u16_be, 10> username;
		u16_be networkNodeID;
	};
	static_assert(sizeof(NodeInfoFromBeacon) == 30, "Invalid size for NWM::NodeInfoFromBeacon");

	struct NodeInfoLE {
		u64_le friendCodeSeed = 0;
		std::array<u16_le, 10> username = {};
		u8 unk1[4];
		u16_le networkNodeID = 0;
		u8 unk2[6];
	};
	static_assert(sizeof(NodeInfoLE) == 40, "Invalid size for NWM::NodeInfoLE");

#pragma pack(pop)

}  // namespace NWM