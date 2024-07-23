#pragma once
#include <optional>

#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

class ACService {
	using Handle = HorizonHandle;

	Handle handle = KernelHandles::AC;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, acLogger)

	// Service commands
	void cancelConnectAsync(u32 messagePointer);
	void closeAsync(u32 messagePointer);
	void createDefaultConfig(u32 messagePointer);
	void getConnectingInfraPriority(u32 messagePointer);
	void getStatus(u32 messagePointer);
	void getLastErrorCode(u32 messagePointer);
	void getWifiStatus(u32 messagePointer);
	void isConnected(u32 messagePointer);
	void registerDisconnectEvent(u32 messagePointer);
	void setClientVersion(u32 messagePointer);

	bool connected = false;
	std::optional<Handle> disconnectEvent = std::nullopt;

  public:
	ACService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};