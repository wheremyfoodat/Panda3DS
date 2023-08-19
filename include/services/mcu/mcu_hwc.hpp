#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

namespace MCU {
	class HWCService {
		Handle handle = KernelHandles::MCU_HWC;
		Memory& mem;
		MAKE_LOG_FUNCTION(log, mcuLogger)

		// Service commands
		void getBatteryLevel(u32 messagePointer);

	  public:
		HWCService(Memory& mem) : mem(mem) {}
		void reset();
		void handleSyncRequest(u32 messagePointer);
	};
}  // namespace MCU