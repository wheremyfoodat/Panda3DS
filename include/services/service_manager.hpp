#pragma once
#include <array>
#include "helpers.hpp"
#include "memory.hpp"

class ServiceManager {
	std::array<u32, 16>& regs;
	Memory& mem;

	// "srv:" commands
	void getServiceHandle(u32 messagePointer);
	void registerClient(u32 messagePointer);

public:
	ServiceManager(std::array<u32, 16>& regs, Memory& mem);
	void reset();
	void handleSyncRequest(u32 TLSPointer);
};