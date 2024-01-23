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
	using Event = std::optional<Handle>;

	struct Port {
		Event bufferErrorInterruptEvent = std::nullopt;
		Event receiveEvent = std::nullopt;
		u16 transferBytes;

		void reset() {
			bufferErrorInterruptEvent = std::nullopt;
			receiveEvent = std::nullopt;
			transferBytes = 256;
		}
	};

	Handle handle = KernelHandles::CAM;
	Memory& mem;
	Kernel& kernel;
	MAKE_LOG_FUNCTION(log, camLogger)

	static constexpr size_t portCount = 2;
	std::array<Port, portCount> ports;

	// Service commands
	void driverInitialize(u32 messagePointer);
	void driverFinalize(u32 messagePointer);
	void getMaxBytes(u32 messagePointer);
	void getMaxLines(u32 messagePointer);
	void getBufferErrorInterruptEvent(u32 messagePointer);
	void getSuitableY2RCoefficients(u32 messagePointer);
	void getTransferBytes(u32 messagePointer);
	void setContrast(u32 messagePointer);
	void setFrameRate(u32 messagePointer);
	void setReceiving(u32 messagePointer);
	void setSize(u32 messagePointer);
	void setTransferBytes(u32 messagePointer);
	void setTransferLines(u32 messagePointer);
	void setTrimming(u32 messagePointer);
	void setTrimmingParamsCenter(u32 messagePointer);
	void startCapture(u32 messagePointer);

  public:
	CAMService(Memory& mem, Kernel& kernel) : mem(mem), kernel(kernel) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};