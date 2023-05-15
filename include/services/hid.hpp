#pragma once
#include <array>
#include <optional>
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

// Circular dependency because we need HID to spawn events
class Kernel;

class HIDService {
	Handle handle = KernelHandles::HID;
	Memory& mem;
	Kernel& kernel;
	u8* sharedMem = nullptr; // Pointer to HID shared memory

	bool accelerometerEnabled;
	bool eventsInitialized;
	bool gyroEnabled;

	std::array<std::optional<Handle>, 5> events;

	MAKE_LOG_FUNCTION(log, hidLogger)

	// Service commands
	void enableAccelerometer(u32 messagePointer);
	void enableGyroscopeLow(u32 messagePointer);
	void getGyroscopeLowCalibrateParam(u32 messagePointer);
	void getGyroscopeCoefficient(u32 messagePointer);
	void getIPCHandles(u32 messagePointer);

public:
	HIDService(Memory& mem, Kernel& kernel) : mem(mem), kernel(kernel) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
	void updateInputs();

	void setSharedMem(u8* ptr) {
		sharedMem = ptr;
		if (ptr != nullptr) { // Zero-fill shared memory in case the process tries to read stale service data or vice versa
			std::memset(ptr, 0, 0x2b0);
		}
	}
};