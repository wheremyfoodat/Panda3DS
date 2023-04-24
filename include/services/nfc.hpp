#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

class NFCService {
	Handle handle = KernelHandles::NFC;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, nfcLogger)

	// Service commands
	void initialize(u32 messagePointer);

public:
	NFCService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};