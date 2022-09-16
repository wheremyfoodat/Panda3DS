#pragma once
#include "kernel_types.hpp"

// Values and resource limit structure taken from Citra

struct ResourceLimitValues {
	u32 maxPriority;
	u32 maxCommit;
	u32 maxThreads;
	u32 maxEvents;
	u32 maxMutexes;
	u32 maxSemaphores;
	u32 maxTimers;
	u32 maxSharedMem;
	u32 maxAddressArbiters;
	u32 maxCPUTime;
};

// APPLICATION resource limit
static constexpr ResourceLimitValues appResourceLimits = {
	.maxPriority = 0x18,
	.maxCommit = 0x4000000,
	.maxThreads = 0x20,
	.maxEvents = 0x20,
	.maxMutexes = 0x20,
	.maxSemaphores = 0x8,
	.maxTimers = 0x8,
	.maxSharedMem = 0x20,
	.maxAddressArbiters = 0x2,
	.maxCPUTime = 0x1E
};

// SYS_APPLET resource limit
static constexpr ResourceLimitValues sysAppletResourceLimits = {
	.maxPriority = 0x4,
	.maxCommit = 0x5E00000,
	.maxThreads = 0x1D,
	.maxEvents = 0xB,
	.maxMutexes = 0x8,
	.maxSemaphores = 0x4,
	.maxTimers = 0x4,
	.maxSharedMem = 0x8,
	.maxAddressArbiters = 0x3,
	.maxCPUTime = 0x2710
};

// LIB_APPLET resource limit
static constexpr ResourceLimitValues libAppletResourceLimits = {
	.maxPriority = 0x4,
	.maxCommit = 0x600000,
	.maxThreads = 0xE,
	.maxEvents = 0x8,
	.maxMutexes = 0x8,
	.maxSemaphores = 0x4,
	.maxTimers = 0x4,
	.maxSharedMem = 0x8,
	.maxAddressArbiters = 0x1,
	.maxCPUTime = 0x2710
};

// OTHER resource limit
static constexpr ResourceLimitValues otherResourceLimits = {
	.maxPriority = 0x4,
	.maxCommit = 0x2180000,
	.maxThreads = 0xE1,
	.maxEvents = 0x108,
	.maxMutexes = 0x25,
	.maxSemaphores = 0x43,
	.maxTimers = 0x2C,
	.maxSharedMem = 0x1F,
	.maxAddressArbiters = 0x2D,
	.maxCPUTime = 0x3E8
};

namespace ResourceType {
	enum : u32 {
		Priority = 0,
		Commit = 1,
		Thread = 2,
		Events = 3,
		Mutex = 4,
		Semaphore = 5,
		Timer = 6,
		SharedMem = 7,
		AddressArbiter = 8,
		CPUTime = 9
	};
}