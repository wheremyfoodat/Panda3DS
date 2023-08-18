#pragma once
#include "config.hpp"
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

class PTMService {
	Handle handle = KernelHandles::PTM;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, ptmLogger)

	const EmulatorConfig& config;

	// Service commands
	void configureNew3DSCPU(u32 messagePointer);
	void getAdapterState(u32 messagePointer);
	void getBatteryLevel(u32 messagePointer);
	void getStepHistory(u32 messagePointer);
	void getTotalStepCount(u32 messagePointer);

public:
	PTMService(Memory& mem, const EmulatorConfig& config) : mem(mem), config(config) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);

	// 0% -> 0 (shutting down)
	// 1-5% -> 1
	// 6-10% -> 2
	// 11-30% -> 3
	// 31-60% -> 4
	// 61-100% -> 5
	static constexpr u8 batteryPercentToLevel(u8 percent) {
		if (percent == 0) {
			return 0;
		} else if (percent >= 1 && percent <= 5) {
			return 1;
		} else if (percent >= 6 && percent <= 10) {
			return 2;
		} else if (percent >= 11 && percent <= 30) {
			return 3;
		} else if (percent >= 31 && percent <= 60) {
			return 4;
		} else {
			return 5;
		}
	}
};