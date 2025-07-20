#pragma once
#include <optional>

#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

class Kernel;

class CECDService {
	using Handle = HorizonHandle;

	Handle handle = KernelHandles::CECD;
	Memory& mem;
	Kernel& kernel;
	MAKE_LOG_FUNCTION(log, cecdLogger)

	std::optional<Handle> infoEvent;
	std::optional<Handle> changeStateEvent;

	// Service commands
	void getChangeStateEventHandle(u32 messagePointer);
	void getInfoEventHandle(u32 messagePointer);
	void openAndRead(u32 messagePointer);
	void stop(u32 messagePointer);

  public:
	CECDService(Memory& mem, Kernel& kernel) : mem(mem), kernel(kernel) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};