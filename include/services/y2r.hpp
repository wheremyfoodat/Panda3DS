#pragma once
#include <optional>
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

// Circular dependencies go br
class Kernel;

class Y2RService {
	Handle handle = KernelHandles::Y2R;
	Memory& mem;
	Kernel& kernel;
	MAKE_LOG_FUNCTION(log, y2rLogger)

	std::optional<Handle> transferEndEvent;
	bool transferEndInterruptEnabled;

	enum class BusyStatus : u32 {
		NotBusy = 0,
		Busy = 1
	};

	enum class InputFormat : u32 {
		YUV422_Individual8 = 0,
		YUV420_Individual8 = 1,
		YUV422_Individual16 = 2,
		YUV420_Individual16 = 3,
		YUV422_Batch = 4,
	};

	InputFormat inputFmt;

	// Service commands
	void driverInitialize(u32 messagePointer);
	void isBusyConversion(u32 messagePointer);
	void pingProcess(u32 messagePointer);
	void setTransferEndInterrupt(u32 messagePointer);
	void getTransferEndEvent(u32 messagePointer);
	void setInputFormat(u32 messagePointer);
	void stopConversion(u32 messagePointer);

public:
	Y2RService(Memory& mem, Kernel& kernel) : mem(mem), kernel(kernel) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};