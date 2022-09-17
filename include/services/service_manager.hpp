#pragma once
#include <array>
#include "helpers.hpp"
#include "memory.hpp"
#include "services/apt.hpp"

class ServiceManager {
	std::array<u32, 16>& regs;
	Memory& mem;

	APTService apt;

	// "srv:" commands
	void getServiceHandle(u32 messagePointer);
	void registerClient(u32 messagePointer);

public:
	ServiceManager(std::array<u32, 16>& regs, Memory& mem);
	void reset();
	void handleSyncRequest(u32 messagePointer);

	// Forward a SendSyncRequest IPC message to the service with the respective handle
	void sendCommandToService(u32 messagePointer, Handle handle);
};