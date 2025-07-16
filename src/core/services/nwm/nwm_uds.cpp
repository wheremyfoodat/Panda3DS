#include "services/nwm/nwm_uds.hpp"

#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <thread>
#include <vector>

#include "ipc.hpp"
#include "kernel.hpp"
#include "services/nwm/nwm_types.hpp"
#include "sockpp/platform.h"
#include "sockpp/udp_socket.h"

using namespace NWM;

bool sockppInitialized = false;

auto beaconSendPort = sockpp::TEST_PORT;
auto serverReceivePort = in_port_t(sockpp::TEST_PORT + 1);
NWM::Beacon sendBeacon;

// static constexpr std::array<u8, 6> MACAddress = {0x40, 0xF4, 0x07, 0xFF, 0xFF, 0xEE};
static constexpr std::array<u8, 6> MACAddress = {0, 0, 0, 0, 0, 0};

namespace NWMCommands {
	enum : u32 {
		Finalize = 0x00030000,
		GetConnectionStatus = 0x000B0000,
		StartScan = 0x000F0404,
		Bind = 0x00120100,
		Unbind = 0x00130040,
		PullPacket = 0x001400C0,
		SendTo = 0x00170182,
		GetChannel = 0x001A0000,
		InitializeWithVersion = 0x001B0302,
		CreateNetwork2 = 0x001D0044,
		GetNodeInformationList = 0x001F0006,
	};
}

void NwmUdsService::reset() {
	connectionStatus = {};
	networkInfo = {};

	eventHandle = std::nullopt;
	initialized = false;
}

void NwmUdsService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);

	switch (command) {
		case NWMCommands::InitializeWithVersion: initializeWithVersion(messagePointer); break;
		case NWMCommands::Finalize: finalize(messagePointer); break;
		case NWMCommands::CreateNetwork2: createNetwork2(messagePointer); break;
		case NWMCommands::GetConnectionStatus: getConnectionStatus(messagePointer); break;
		case NWMCommands::PullPacket: pullPacket(messagePointer); break;
		case NWMCommands::SendTo: sendTo(messagePointer); break;
		case NWMCommands::StartScan: startScan(messagePointer); break;
		case NWMCommands::Bind: bind(messagePointer); break;
		case NWMCommands::Unbind: unbind(messagePointer); break;
		case NWMCommands::GetChannel: getChannel(messagePointer); break;
		case NWMCommands::GetNodeInformationList: getNodeInformationList(messagePointer); break;

		default:
			Helpers::warn("NWM::UDS service requested. Command: %08X\n", command);
			mem.write32(messagePointer + 4, Result::Success);
			break;
	}
}

void NwmUdsService::initializeWithVersion(u32 messagePointer) {
	Helpers::warn("Initializing NWM::UDS (Local multiplayer, unimplemented)\n");
	log("NWM::UDS::InitializeWithVersion()\n");

	if (!eventHandle.has_value()) {
		eventHandle = kernel.makeEvent(ResetType::OneShot);
	}

	if (initialized) {
		printf("NWM::UDS initialized twice\n");
	}

	initialized = true;

	// Stubbed to fail temporarily, since some games will break trying to establish networks otherwise
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);
	mem.write32(messagePointer + 12, *eventHandle);
}

void NwmUdsService::bind(u32 messagePointer) {
	Helpers::warn("Called NWM::UDS::Bind");
	log("NWM::UDS::Bind()\n");

	mem.write32(messagePointer + 4, Result::Success);
}

void NwmUdsService::createNetwork2(u32 messagePointer) {
	const u32 bufferSize = mem.read32(messagePointer + 8) >> 12;
	const u32 inputPointer = mem.read32(messagePointer + 12);

	Helpers::warn("Called NWM::UDS::CreateNetwork2");
	log("NWM::UDS::CreateNetwork2()\n");

	mem.write32(messagePointer, IPC::responseHeader(0x1D, 1, 0));

	// Seems like games don't actually pass the buffer size properly? Does NWM just ignore it?
	if (bufferSize != sizeof(NetworkInfo)) {
		// Helpers::warn("NWM::UDS::CreateNetwork2: Invalid size for the network info struct.");
		// mem.write32(messagePointer + 4, Result::FailurePlaceholder);
		// return;
	}

	// Read the NetworkInfo struct the game passed us
	std::vector<u8> data;
	data.reserve(sizeof(NetworkInfo));

	for (u32 i = 0; i < sizeof(NetworkInfo); i++) {
		data.push_back(mem.read8(inputPointer + i));
	}

	std::memcpy(&networkInfo, data.data(), data.size());
	fmt::print("Netmrow ID: {}\n", u32(*(u32_be*)&data[24]));

	// Make a network with us as the host. There's only 1 node (us, index = 1) and we've got id = 1.
	connectionStatus.status = ConnectionState::ConnectedAsHost;
	connectionStatus.statusChangeReason = ConnectionStateChangeReason::ConnectionEstablished;
	connectionStatus.networkNodeID = 1;
	connectionStatus.totalNodes = 1;
	connectionStatus.nodes[0] = connectionStatus.networkNodeID;
	connectionStatus.maxNodes = networkInfo.maxNodes;

	// Set up misc network info fields (MAC address, OUI, node info...)
	networkInfo.totalNodes = 1;
	networkInfo.hostMAC = MACAddress;
	networkInfo.oui = NWM::nintendoOUI;
	networkInfo.ouiType = u8(NWM::VendorSpecificTagID::NetworkInfo);

	if (networkInfo.channel == 0) {
		networkInfo.channel = NWM::defaultNetworkChannel;
	}

	// Set that node 0 of the network (Us) is taken
	markNodeAsConnected(0);

	// Signal the connection event after setting up the network
	if (eventHandle) {
		kernel.signalEvent(*eventHandle);
	}

	setupUDPServer();
	mem.write32(messagePointer + 4, Result::Success);
}

void NwmUdsService::getConnectionStatus(u32 messagePointer) {
	Helpers::warn("Called NWM::UDS::GetConnectionStatus)");
	log("NWM::UDS::GetConnectionStatus()\n");

	mem.write32(messagePointer, IPC::responseHeader(0x0B, 13, 0));
	mem.write32(messagePointer + 4, Result::Success);

	// Write the connection status to the IPC buffer
	const u32* words = (u32*)&connectionStatus;
	for (u32 offset = 0; offset < 0x30; offset += 4) {
		mem.write32(messagePointer + 8 + offset, *words++);
	}
}

void NwmUdsService::getChannel(u32 messagePointer) {
	Helpers::warn("Called NWM::UDS::GetChannel");
	log("NWM::UDS::GetChannel()\n");
	const bool connected = true;

	mem.write32(messagePointer, IPC::responseHeader(0x1A, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, connected ? 1 : 0);

	/*
	kernel.signalEvent(*eventHandle);
	markNodeAsConnected(1);
	connectionStatus.nodes[1] = 1 + 1;
	connectionStatus.totalNodes++;
	*/
}

void NwmUdsService::pullPacket(u32 messagePointer) {
	Helpers::warn("Called NWM::UDS::PullPacket");
	log("NWM::UDS::PullPacket()\n");

	mem.write32(messagePointer + 4, Result::Success);
}

void NwmUdsService::sendTo(u32 messagePointer) {
	Helpers::warn("Called NWM::UDS::SendTo\n");
	log("NWM::UDS::SendTo()\n");

	mem.write32(messagePointer + 4, Result::Success);
}

void NwmUdsService::unbind(u32 messagePointer) {
	Helpers::warn("Called NWM::UDS::Unbind\n");
	log("NWM::UDS::Unbind()\n");

	mem.write32(messagePointer + 4, Result::Success);
}

void NwmUdsService::finalize(u32 messagePointer) {
	Helpers::warn("Called NWM::UDS::Finalize\n");
	log("NWM::UDS::Finalize()\n");

	mem.write32(messagePointer, IPC::responseHeader(0x03, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NwmUdsService::startScan(u32 messagePointer) {
	Helpers::warn("Called NWM::UDS::StartScan\n");
	log("NWM::UDS::StartScan()\n");

	const u32 outputBufferSize = mem.read32(messagePointer + 4);
	const u32 macLow = mem.read32(messagePointer + 16);
	const u32 macHigh = mem.read32(messagePointer + 20);
	const u32 wlanCommID = mem.read32(messagePointer + 60);
	const u32 id = mem.read32(messagePointer + 64);
	const u32 beaconReplyBuffer = mem.read32(messagePointer + 80);

	sockpp::udp_socket recvSock;

	if (!recvSock.bind(sockpp::inet_address("0.0.0.0", beaconSendPort))) {
		fmt::print("Failed to bind scan socket\n");
		mem.write32(messagePointer + 4, Result::FailurePlaceholder);
		return;
	}

	std::vector<std::vector<u8>> foundBeacons;
	// The starting beacon data size is 3 u32s for the BeaconDataReply structure header
	usize beaconDataSize = 3 * sizeof(u32);

	const auto start = std::chrono::steady_clock::now();

	// Hack: Scan for beacons for 1 second, return them all at once
	while (std::chrono::steady_clock::now() - start < std::chrono::seconds(1)) {
		u8 buf[2048];
		sockpp::inet_address from;
		auto len = recvSock.recv_from(buf, sizeof(buf), &from);

		if (len && len.value() > 0) {
			std::vector<u8> beaconData(buf, buf + len.value());
			foundBeacons.push_back(beaconData);

			// Each beacon entry includes 0x1C beacon entry header + the actual beacon data
			beaconDataSize += beaconData.size() + 0x1C;
			fmt::print("Received beacon from {}\nData: {}\n", from.to_string(), beaconData);
			break;
		}
	}

	mem.write32(messagePointer, IPC::responseHeader(0x0F, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);

	// Max output size, from the command request.
	mem.write32(beaconReplyBuffer, outputBufferSize);
	// Total amount of output data written relative to struct+0. 0xC when there's no entries.
	mem.write32(beaconReplyBuffer + 4, beaconDataSize);
	// Total entries, 0 for none.
	mem.write32(beaconReplyBuffer + 8, foundBeacons.size());

	u32 beaconReplyAddr = beaconReplyBuffer + 12;

	for (usize i = 0; i < foundBeacons.size(); i++) {
		const auto& beacon = foundBeacons[i];
		// Size of this entire entry, including the 0x1C-byte header
		const u32 entrySize = u32(beacon.size() + 0x1C);
		mem.write32(beaconReplyAddr, entrySize);

		// AP wifi channel
		mem.write8(beaconReplyAddr + 0x5, 1);

		// AP MAC address
		for (int j = 0; j < 6; j++) {
			mem.write8(beaconReplyAddr + 0x8 + j, MACAddress[j]);
		}

		mem.write32(beaconReplyAddr + 0x14, entrySize);
		// Size of the header
		mem.write32(beaconReplyAddr + 0x18, 0x1C);

		// Advance the reply pointer to go to the beacon data
		beaconReplyAddr += 0x1C;

		// Actual beacon data
		for (usize j = 0; j < beacon.size(); j++) {
			mem.write8(beaconReplyAddr + j, beacon[j]);
		}

		// Advance the reply pointer to go to the next entry
		beaconReplyAddr += beacon.size();
	}
}

void NwmUdsService::markNodeAsConnected(int index) {
	connectionStatus.changedNodes |= (u32(1) << index);
	connectionStatus.nodeBitmap |= (u32(1) << index);
}

void NwmUdsService::markNodeAsDisconnected(int index) {
	connectionStatus.changedNodes |= (u32(1) << index);
	connectionStatus.nodeBitmap &= ~(u32(1) << index);
}

// Decrypts the NDM beacon data to get information about the nodes connected to the AP
void NwmUdsService::getNodeInformationList(u32 messagePointer) {
	Helpers::warn("Called NWM::UDS::GetNodeInformationList\n");
	log("NWM::UDS::GetNodeInformationList()\n");

	const u32 networkInfoInputPtr = mem.read32(messagePointer + 8);
	const u32 tag0Size = mem.read32(messagePointer + 12) >> 12;
	const u32 tag0BufferPtr = mem.read32(messagePointer + 16);
	const u32 tag1Size = mem.read32(messagePointer + 20) >> 12;
	const u32 tag1BufferPtr = mem.read32(messagePointer + 24);

	u32 outputBufferPtr = mem.read32(messagePointer + 0x104);
	// The size of the output buffer is hardcoded to 0x280. We zero-fill the info for the nodes we haven't filled.
	u32 outputBufferEnd = outputBufferPtr + 0x280;

	mem.write32(messagePointer, IPC::responseHeader(0x1F, 1, 2));

	if (!initialized) {
		Helpers::warn("NWM::UDS::GetNodeInformationList: NWM has not been initialized");
		mem.write32(messagePointer + 4, Result::FailurePlaceholder);
		return;
	}

	if (tag0Size < 4 || tag1Size < 4) {
		// 3DBrew says the tag size is hardcoded, but check to make sure it's valid regardless
		Helpers::warn("NWM::UDS::GetNodeInformationList: Wrong tag size");
		mem.write32(messagePointer + 4, Result::FailurePlaceholder);
		return;
	}

	NetworkInfo inputNetworkInfo;
	u8* networkInfoOutputPtr = (u8*)&inputNetworkInfo;

	std::vector<u8> tag0(tag0Size);
	std::vector<u8> tag1(tag1Size);

	// The sizes of the input/output buffers are hardcoded

	// Read network info struct
	for (usize i = 0; i < sizeof(NetworkInfo); i++) {
		networkInfoOutputPtr[i] = mem.read8(networkInfoInputPtr + i);
	}

	fmt::print("Decrypting beacon. MAC: {}\n", inputNetworkInfo.hostMAC);

	// Read tags. Each tag starts with a 4 byte header (3 bytes with the Nintendo OUI, 1 byte with the tag encryption type)
	// And the rest of it is beacon data that we must decrypt
	for (usize i = 0; i < tag0Size; i++) {
		tag0[i] = mem.read8(tag0BufferPtr + i);
	}

	for (usize i = 0; i < tag1Size; i++) {
		tag1[i] = mem.read8(tag1BufferPtr + i);
	}

	fmt::print("Tag0 addr: {:08X}, size: {:08X}\n", tag0BufferPtr, tag0Size);
	fmt::print("This should be the Nintendo OUI: {:02X} {:02X} {:02X}\n", tag0[0], tag0[1], tag0[2]);

	// Store the beacon data in a vector and decrypt it
	std::vector<u8> beacon;
	beacon.insert(beacon.end(), tag0.begin() + 4, tag0.end());
	beacon.insert(beacon.end(), tag1.begin() + 4, tag1.end());

	AESKey key = getBeaconKey();
	AESKey iv = getBeaconIV(inputNetworkInfo);
	fmt::print("Beacon decryption IV: {}\n", iv);

	CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption d(key.data(), key.size(), iv.data());
	// Decrypt buffer in-place
	d.ProcessData(beacon.data(), beacon.data(), beacon.size());

	// The beacon buffer now contains the decrypted beacon data. Parse and return the node info
	std::vector<NWM::NodeInfoFromBeacon> nodeInfo(inputNetworkInfo.maxNodes);

	for (usize i = 0; i < nodeInfo.size(); i++) {
		auto& node = nodeInfo[i];
		std::array<u8, sizeof(node)> nodeInfoBytes;

		// Read the node info from the decrypted beacon, write it to the output buffer
		for (usize j = 0; j < sizeof(node); j++) {
			static constexpr usize beaconDataHeaderSize = 0x12;

			nodeInfoBytes[j] = beacon[beaconDataHeaderSize + i * sizeof(node) + j];
		}

		// Copy the raw bytes to our node object
		std::memcpy(&node, nodeInfoBytes.data(), sizeof(node));

		// GetNodeInformationList byteswaps the node info from BE to LE
		NodeInfoLE nodeInfoSwapped;
		nodeInfoSwapped.networkNodeID = node.networkNodeID;
		nodeInfoSwapped.friendCodeSeed = node.friendCodeSeed;

		for (usize j = 0; j < node.username.size(); j++) {
			nodeInfoSwapped.username[j] = node.username[j];
		}

		fmt::print("Node {} info:\n\tUsername: {}\n\tNetwork node ID: {}\n", i, nodeInfoSwapped.username, nodeInfoSwapped.networkNodeID);

		// Write out the byteswapped bytes to the output pointer
		u8* nodeInfoSwappedBytes = (u8*)&nodeInfoSwapped;
		for (usize j = 0; j < sizeof(nodeInfoSwapped); j++) {
			mem.write8(outputBufferPtr++, nodeInfoSwappedBytes[j]);
		}
	}

	// Zero fill the rest of the output buffer
	while (outputBufferPtr < outputBufferEnd) {
		mem.write32(outputBufferPtr++, 0);
	}

	mem.write32(messagePointer + 4, Result::Success);
}

AESKey NwmUdsService::getBeaconKey() {
	// Use a key of all zeroes for now. Transferring beacons with a real console requires the proper key to be used.
	return {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	};
}

// The IV is made from just sticking the identification parts of the network into a 16-byte buffer
AESKey NwmUdsService::getBeaconIV(const NetworkInfo& networkInfo) {
	AESKey iv;
	iv.fill(0);

	// We have to byteswap the network and WLAN ID from BE to LE before using them in the IV
	u32_le networkID = networkInfo.networkID;
	u32_le wlanID = networkInfo.wlanID;

	std::memcpy(&iv[0], &networkInfo.hostMAC, sizeof(networkInfo.hostMAC));
	std::memcpy(&iv[6], &wlanID, sizeof(wlanID));
	std::memcpy(&iv[10], &networkInfo.id, sizeof(networkInfo.id));
	std::memcpy(&iv[12], &networkID, sizeof(networkID));

	return iv;
}

template <typename UDPSocket>
void NwmUdsService::broadcastBeacons(UDPSocket beaconSocket) {
	char buf[512];

	sockpp::inet_address broadcastAddr("255.255.255.255", beaconSendPort);

	// Broadcast beacons every 200 ms
	while (true) {
		auto key = getBeaconKey();
		auto iv = getBeaconIV(networkInfo);
		auto data = sendBeacon.generate(networkInfo, key, iv);
		fmt::print("Beacon data: {}\n", data);

		beaconSocket.send_to(data.data(), data.size(), broadcastAddr);
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}
}

bool NwmUdsService::setupUDPServer() {
	if (!sockppInitialized) {
		sockppInitialized = true;
		sockpp::initialize();
	}

	auto host = "0.0.0.0";
	sockpp::udp_socket udpsock;
	if (auto res = udpsock.bind(sockpp::inet_address(host, serverReceivePort)); !res) {
		fmt::print("Error binding the UDP v4 socket: {}\n", res.error_message());
		return false;
	}

	udpsock.set_option(SOL_SOCKET, SO_BROADCAST, true);

	std::thread thr(&NwmUdsService::broadcastBeacons<sockpp::udp_socket>, this, std::move(udpsock));
	thr.detach();
	return true;
}