#pragma once
#include <optional>

#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "services/nwm/nwm_types.hpp"

// More circular dependencies
class Kernel;

class NwmUdsService {
	using Handle = HorizonHandle;

	Handle handle = KernelHandles::NWM_UDS;
	Memory& mem;
	Kernel& kernel;

	NWM::ConnectionStatus connectionStatus;
	NWM::NetworkInfo networkInfo;
	MAKE_LOG_FUNCTION(log, nwmUdsLogger)

	bool initialized = false;
	std::optional<Handle> eventHandle = std::nullopt;

	// Service commands
	void bind(u32 messagePointer);
	void createNetwork2(u32 messagePointer);
	void finalize(u32 messagePointer);
	void getConnectionStatus(u32 messagePointer);
	void getChannel(u32 messagePointer);
	void getNodeInformationList(u32 messagePointer);
	void initializeWithVersion(u32 messagePointer);
	void pullPacket(u32 messagePointer);
	void sendTo(u32 messagePointer);
	void startScan(u32 messagePointer);
	void unbind(u32 messagePointer);

	// Get the key and IV for encrypting & decrypting beacon data
	NWM::AESKey getBeaconKey();
	NWM::AESKey getBeaconIV(const NWM::NetworkInfo& networkInfo);

	// Mark nodes as connected or disconnected in our connection status struct
	void markNodeAsConnected(int index);
	void markNodeAsDisconnected(int index);

	// Sets up a UDP server for the network host. Returns true on success.
	bool setupUDPServer();

	template <typename UDPSocket>
	void broadcastBeacons(UDPSocket socket);

  public:
	NwmUdsService(Memory& mem, Kernel& kernel) : mem(mem), kernel(kernel) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};