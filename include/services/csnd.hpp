#pragma once
#include <optional>

#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

// Circular dependencies ^-^
class Kernel;

class CSNDService {
	Handle handle = KernelHandles::CSND;
	Memory& mem;
	Kernel& kernel;
	MAKE_LOG_FUNCTION(log, csndLogger)

	bool initialized = false;
	std::optional<Handle> csndMutex = std::nullopt;

	// Service functions
	void initialize(u32 messagePointer);

  public:
	CSNDService(Memory& mem, Kernel& kernel) : mem(mem), kernel(kernel) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};