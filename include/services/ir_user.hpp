#pragma once
#include <optional>

#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

// Circular dependencies in this project? Never
class Kernel;

class IRUserService {
	Handle handle = KernelHandles::IR_USER;
	Memory& mem;
	Kernel& kernel;
	MAKE_LOG_FUNCTION(log, irUserLogger)

	// Service commands
	void disconnect(u32 messagePointer);
	void finalizeIrnop(u32 messagePointer);
	void getConnectionStatusEvent(u32 messagePointer);
	void initializeIrnopShared(u32 messagePointer);
	void requireConnection(u32 messagePointer);

	std::optional<Handle> connectionStatusEvent = std::nullopt;

  public:
	IRUserService(Memory& mem, Kernel& kernel) : mem(mem), kernel(kernel) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};