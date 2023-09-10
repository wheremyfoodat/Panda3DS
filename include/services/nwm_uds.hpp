#pragma once
#include <optional>

#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

// More circular dependencies
class Kernel;

class NwmUdsService {
	Handle handle = KernelHandles::NWM_UDS;
	Memory& mem;
	Kernel& kernel;
	MAKE_LOG_FUNCTION(log, nwmUdsLogger)

	bool initialized = false;
	std::optional<Handle> eventHandle = std::nullopt;

	// Service commands
	void initializeWithVersion(u32 messagePointer);

  public:
	NwmUdsService(Memory& mem, Kernel& kernel) : mem(mem), kernel(kernel) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};