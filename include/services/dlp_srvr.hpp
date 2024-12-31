#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

// Please forgive me for how everything in this file is named
// "dlp:SRVR" is not a nice name to work with
class DlpSrvrService {
	using Handle = HorizonHandle;

	Handle handle = KernelHandles::DLP_SRVR;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, dlpSrvrLogger)

	// Service commands
	void isChild(u32 messagePointer);

  public:
	DlpSrvrService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};