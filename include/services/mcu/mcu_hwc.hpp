#pragma once
#include "config.hpp"
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

namespace MCU {
	class HWCService {
		HandleType handle = KernelHandles::MCU_HWC;
		Memory& mem;
		MAKE_LOG_FUNCTION(log, mcuLogger)

		const EmulatorConfig& config;

		// Service commands
		void getBatteryLevel(u32 messagePointer);

	  public:
		HWCService(Memory& mem, const EmulatorConfig& config) : mem(mem), config(config) {}
		void reset();
		void handleSyncRequest(u32 messagePointer);
	};
}  // namespace MCU
