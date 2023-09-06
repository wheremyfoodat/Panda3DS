#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

class HTTPService {
	Handle handle = KernelHandles::HTTP;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, httpLogger)

	bool initialized = false;

	// Service commands
	void createRootCertChain(u32 messagePointer);
	void initialize(u32 messagePointer);
	void rootCertChainAddDefaultCert(u32 messagePointer);

  public:
	HTTPService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};