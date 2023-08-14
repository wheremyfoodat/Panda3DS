#pragma once
#include <array>
#include <optional>

#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

// Yay, circular dependencies!
class Kernel;

class CAMService {
	Handle handle = KernelHandles::CAM;
	Memory& mem;
	Kernel& kernel;
	MAKE_LOG_FUNCTION(log, camLogger)

	using Event = std::optional<Handle>;
	static constexpr size_t portCount = 4; // PORT_NONE, PORT_CAM1, PORT_CAM2, PORT_BOTH
	std::array<Event, portCount> bufferErrorInterruptEvents;

	// Service commands
	void driverInitialize(u32 messagePointer);
	void getMaxLines(u32 messagePointer);
	void getBufferErrorInterruptEvent(u32 messagePointer);

  public:
	CAMService(Memory& mem, Kernel& kernel) : mem(mem), kernel(kernel) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};