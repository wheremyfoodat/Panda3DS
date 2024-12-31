#pragma once
#include <optional>

#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

// Circular dependencies ^-^
class Kernel;

class CSNDService {
	using Handle = HorizonHandle;

	Handle handle = KernelHandles::CSND;
	Memory& mem;
	Kernel& kernel;
	MAKE_LOG_FUNCTION(log, csndLogger)

	u8* sharedMemory = nullptr;
	std::optional<Handle> csndMutex = std::nullopt;
	size_t sharedMemSize = 0;
	bool initialized = false;

	// Service functions
	void acquireSoundChannels(u32 messagePointer);
	void executeCommands(u32 messagePointer);
	void initialize(u32 messagePointer);

  public:
	CSNDService(Memory& mem, Kernel& kernel) : mem(mem), kernel(kernel) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);

	void setSharedMemory(u8* ptr) { sharedMemory = ptr; }
};